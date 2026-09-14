"""Prepare local Starfield libraries and schema defaults for the Ruffle host."""

import argparse
import json
from pathlib import Path, PurePosixPath
import struct
import xml.etree.ElementTree as ET
import zlib

from preview_abc import adapt_callbacks


def swf_tags(source):
    if source[:3] == b"CWS":
        body = zlib.decompress(source[8:])
    elif source[:3] == b"FWS":
        body = source[8:]
    else:
        raise ValueError("Expected a CWS or FWS movie")
    if len(body) + 8 != struct.unpack_from("<I", source, 4)[0]:
        raise ValueError("Invalid SWF length")
    offset = (5 + 4 * (body[0] >> 3) + 7) // 8 + 4
    prefix = body[:offset]
    tags = []
    while offset < len(body):
        record = struct.unpack_from("<H", body, offset)[0]
        offset += 2
        size, code = record & 63, record >> 6
        if size == 63:
            size = struct.unpack_from("<I", body, offset)[0]
            offset += 4
        data = body[offset:offset + size]
        if len(data) != size:
            raise ValueError("Truncated SWF tag")
        tags.append((code, data))
        offset += size
    return prefix, tags


def pack_swf(source, prefix, tags):
    body = bytearray(prefix)
    for code, data in tags:
        size = len(data)
        body += struct.pack("<H", code << 6 | min(size, 63))
        if size >= 63:
            body += struct.pack("<I", size)
        body += data
    return b"FWS" + source[3:4] + struct.pack("<I", len(body) + 8) + body


class InterfaceArchive:
    def __init__(self, path):
        self.path = path
        with path.open("rb") as stream:
            magic, version, kind, count, names = struct.unpack("<4sI4sIQ", stream.read(24))
            if magic != b"BTDX" or kind != b"GNRL" or version not in (1, 2):
                raise ValueError("Expected a version 1 or 2 GNRL BA2 archive")
            if version == 2:
                stream.read(8)
            records = [struct.unpack("<I4sIIQIII", stream.read(36)) for _ in range(count)]
            stream.seek(names)
            self.files = {}
            for record in records:
                length = struct.unpack("<H", stream.read(2))[0]
                name = stream.read(length).decode("utf-8").replace("\\", "/").lower()
                self.files[name] = record

    def read(self, name):
        record = self.files["interface/" + name.lower()]
        offset, packed, unpacked = record[4:7]
        with self.path.open("rb") as stream:
            stream.seek(offset)
            data = stream.read(packed or unpacked)
        if packed:
            data = zlib.decompress(data)
        if len(data) != unpacked:
            raise ValueError(f"Invalid asset length: {name}")
        return data


def prepare(archive_path, output, schema_paths, large):
    archive = InterfaceArchive(archive_path)
    assets = output / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    config = ET.Element("preview")
    libraries = ET.SubElement(config, "libraries")
    loaded = set()

    def library(name):
        name = name.lower()
        if name in loaded:
            return
        if PurePosixPath(name).name != name or not name.endswith(".swf"):
            raise ValueError(f"Unexpected library path: {name}")
        loaded.add(name)
        source = archive.read(name)
        prefix, tags = swf_tags(source)
        font_names = {}
        if name == "fonts_en.swf":
            for code, data in tags:
                if code == 76:
                    offset = 2
                    for _ in range(struct.unpack_from("<H", data)[0]):
                        symbol = struct.unpack_from("<H", data, offset)[0]
                        end = data.index(0, offset + 2)
                        font_names[symbol] = data[offset + 2:end]
                        offset = end + 1
            fonts = ET.SubElement(config, "fonts")
            for alias in font_names.values():
                ET.SubElement(fonts, "font", name=alias.decode("ascii"))
        retained = []
        for code, data in tags:
            if code == 82:
                start = data.index(0, 4) + 1
                data = data[:start] + adapt_callbacks(data[start:])
            # Scaleform resolves the exported $aliases through its font library.
            # Register the same outlines under those names in ordinary Flash.
            if code == 75 and font_names:
                alias = font_names[struct.unpack_from("<H", data)[0]]
                data = data[:4] + bytes([len(alias)]) + alias + data[5 + data[4]:]
            if code == 88 and font_names:
                alias = font_names[struct.unpack_from("<H", data)[0]]
                data = data[:2] + alias + data[data.index(0, 2):]
            if code in (57, 71):
                url, symbols = data.split(b"\0", 1)
                if code == 71:
                    symbols = symbols[2:]
                if symbols != b"\0\0":
                    raise ValueError(f"Symbol imports need explicit support: {name}")
                library(url.decode("ascii"))
                # Scaleform's zero-symbol imports are replaced by explicit Loader
                # calls, in dependency order, only in these generated preview copies.
            else:
                retained.append((code, data))
        (assets / name).write_bytes(pack_swf(source, prefix, retained))
        ET.SubElement(libraries, "library", url="assets/" + name)
        print("Prepared", name)

    library("SettingsPanel_LRG.swf" if large else "SettingsPanel.swf")
    rows = ET.SubElement(config, "rows")
    for path in schema_paths:
        schema = json.loads(path.read_text(encoding="utf-8-sig"))
        for group in schema["groups"]:
            for setting in group["settings"]:
                if setting["type"] != "bool" or not isinstance(setting["default"], bool):
                    raise ValueError(f"Preview supports boolean settings only: {path}")
                ET.SubElement(rows, "row", mod=schema["id"], modTitle=schema["title"],
                              modDescription=schema.get("description", ""),
                              group=group["id"], groupTitle=group["label"], key=setting["key"],
                              title=setting["label"], hint=setting.get("hint", ""),
                              value=str(setting["default"]).lower())
    ET.indent(config)
    ET.ElementTree(config).write(output / "preview.xml", encoding="utf-8", xml_declaration=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--large", action="store_true")
    parser.add_argument("schemas", type=Path, nargs="+")
    args = parser.parse_args()
    prepare(args.archive, args.output, args.schemas, args.large)
