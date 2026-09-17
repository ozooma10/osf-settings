"""Prepare local Starfield libraries and schema defaults for the Ruffle host."""

import argparse
from functools import cache
import json
import math
from pathlib import Path, PurePosixPath
import re
import struct
import xml.etree.ElementTree as ET
import zlib

from preview_abc import adapt_callbacks


@cache
def virtual_key_names():
    # The offline preview reads the same enum used by native magic_enum reflection.
    header = Path(__file__).resolve().parents[1] / "lib/commonlibsf/lib/commonlib-shared/include/REX/W32/USER32.h"
    source = header.read_text(encoding="utf-8")
    enum = re.search(r"\benum\s+VK\s*:\s*std::uint32_t\s*\{([^}]+)\}", source)
    if enum is None:
        raise ValueError("Cannot find CommonLib's VK enum for the preview")
    names = {}
    for declaration in enum[1].split(","):
        if not declaration.strip():
            continue
        field = re.fullmatch(r"\s*(VK_\w+)\s*=\s*(0x[0-9a-fA-F]+|VK_\w+)\s*", declaration)
        if field is None:
            raise ValueError(f"Unsupported VK enum declaration: {declaration.strip()}")
        names[field[1]] = names[field[2]] if field[2].startswith("VK_") else int(field[2], 16)
    return names


def key_default(value, allow_unbound=False):
    # Match KeyCodeFromName's stable schema aliases; runtime and preview rows stay numeric.
    if isinstance(value, str):
        name = value.upper() if value.isascii() else ""
        name = name.removeprefix("VK_")
        aliases = {
            "BACKSPACE": "BACK", "ENTER": "RETURN", "CAPSLOCK": "CAPITAL",
            "PAGEUP": "PRIOR", "PAGEDOWN": "NEXT", "PRINTSCREEN": "SNAPSHOT",
            "SCROLLLOCK": "SCROLL", "LCTRL": "LCONTROL", "RCTRL": "RCONTROL",
            "LALT": "LMENU", "RALT": "RMENU",
            "NUMPADMULTIPLY": "MULTIPLY", "NUMPADADD": "ADD", "NUMPADSUBTRACT": "SUBTRACT",
            "NUMPADDECIMAL": "DECIMAL", "NUMPADDIVIDE": "DIVIDE",
        }
        value = 255 if name == "UNBOUND" else virtual_key_names().get("VK_" + aliases.get(name, name))
    if (type(value) is not int or type(allow_unbound) is not bool or
            not (value == 255 and allow_unbound or 0 < value < 255 and value not in (1, 2, 4, 5, 6, 27))):
        raise ValueError("Invalid keyboard virtual-key code or key name")
    return value


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


def prepare(archive_path, output, schema_paths, large, menu_path, issues_path=None):
    archive = InterfaceArchive(archive_path)
    assets = output / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    config = ET.Element("preview")
    libraries = ET.SubElement(config, "libraries")
    loaded = set()
    font_families = {}

    def text_font(data):
        offset = 2 + (5 + 4 * (data[2] >> 3) + 7) // 8
        flags = int.from_bytes(data[offset:offset + 2], "big")
        offset += 2 + (2 if flags & 0x0100 else 0)
        if flags & 0x0080:
            # Ruffle treats DefineEditText.FontClass as a family name.
            # Adapt authored fields without renaming the shared fonts.
            end = data.index(0, offset)
            family = font_families.get(data[offset:end])
            if family:
                data = data[:offset] + family + data[end:]
        return data

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
            families = {struct.unpack_from("<H", data)[0]: data[5:5 + data[4]].rstrip(b"\0")
                        for code, data in tags if code == 75}
            for code, data in tags:
                if code == 76:
                    offset = 2
                    for _ in range(struct.unpack_from("<H", data)[0]):
                        symbol = struct.unpack_from("<H", data, offset)[0]
                        end = data.index(0, offset + 2)
                        font_names[symbol] = data[offset + 2:end]
                        font_families[font_names[symbol]] = families[symbol]
                        offset = end + 1
            fonts = ET.SubElement(config, "fonts")
            for alias in font_names.values():
                ET.SubElement(fonts, "font", name=alias.decode("ascii"))
        retained = []
        for code, data in tags:
            if code == 82:
                start = data.index(0, 4) + 1
                data = data[:start] + adapt_callbacks(data[start:])
            # Keep the real font names. Renaming them to exported class names
            # hides invalid TextFormat.font assignments in the production menu.
            if code == 37:
                data = text_font(data)
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
    source = menu_path.read_bytes()
    prefix, tags = swf_tags(source)
    tags = [(code, text_font(data) if code == 37 else data) for code, data in tags]
    (output / "menu.swf").write_bytes(pack_swf(source, prefix, tags))
    rows = ET.SubElement(config, "rows")
    for path in schema_paths:
        schema = json.loads(path.read_text(encoding="utf-8-sig"))
        for group in schema["groups"]:
            for setting in group["settings"]:
                if "requires" in setting and setting["requires"] != "restart":
                    raise ValueError(f'Preview requires must be "restart" when present: {path} / {setting["key"]}')
                kind, default = setting["type"], setting["default"]
                attributes = {"type": kind, "value": str(default).lower(), "editable": "true"}
                if kind == "int" and type(default) is int:
                    minimum, maximum = setting.get("min"), setting.get("max")
                    for bound in (default, minimum, maximum):
                        if bound is not None and (type(bound) is not int or not -(2**63) <= bound < 2**63):
                            raise ValueError(f"Invalid preview integer: {path}")
                    if (minimum is not None and default < minimum) or (maximum is not None and default > maximum):
                        raise ValueError(f"Preview default is outside its bounds: {path}")
                    if minimum is not None:
                        attributes["minimum"] = str(minimum)
                    if maximum is not None:
                        attributes["maximum"] = str(maximum)
                    attributes["editable"] = str(minimum is not None and maximum is not None and
                        -(2**53 - 1) <= minimum < maximum <= 2**53 - 1 and maximum - minimum <= 2**32 - 1).lower()
                elif kind == "float":
                    minimum, maximum, step = setting.get("min"), setting.get("max"), setting.get("step", 0.1)
                    for field in ("default", "min", "max", "step"):
                        if field in setting and (type(setting[field]) not in (int, float) or not math.isfinite(setting[field])):
                            raise ValueError(f"Preview {field} must be a finite number: {path}")
                    if step <= 0 or (minimum is not None and default < minimum) or (maximum is not None and default > maximum):
                        raise ValueError(f"Invalid preview float bounds or step: {path}")
                    if minimum is not None:
                        attributes["minimum"] = str(minimum)
                    if maximum is not None:
                        attributes["maximum"] = str(maximum)
                    attributes.update(float_slider(minimum, maximum, step))
                elif kind == "enum":
                    options = setting.get("options")
                    if (not isinstance(options, list) or not options or
                            any(not isinstance(value, str) or not value for value in options) or
                            len(set(options)) != len(options) or not isinstance(default, str) or default not in options):
                        raise ValueError(f"Invalid preview enum options or default: {path}")
                    labels = setting.get("optionLabels", options)
                    if (not isinstance(labels, list) or len(labels) != len(options) or
                            any(not isinstance(label, str) for label in labels)):
                        raise ValueError(f"Invalid preview enum labels: {path}")
                    attributes.update(value=default, editable=str(len(options) > 1).lower())
                elif kind == "key":
                    allow_unbound = setting.get("allowUnbound", False)
                    try:
                        default = key_default(default, allow_unbound)
                    except ValueError as error:
                        raise ValueError(f"Invalid preview key default: {path} / {setting['key']}") from error
                    attributes.update(value=str(default), allowUnbound=str(allow_unbound).lower())
                elif kind != "bool" or type(default) is not bool:
                    raise ValueError(f"Preview supports boolean, integer, float, enum, and key settings only: {path}")
                row = ET.SubElement(rows, "row", mod=schema["id"], modTitle=schema["title"],
                              modDescription=schema.get("description", ""),
                              group=group["id"], groupTitle=group["label"], key=setting["key"],
                              title=setting["label"], hint=setting.get("hint", ""),
                              requiresRestart=str(setting.get("requires") == "restart").lower(),
                              **attributes)
                if kind == "enum":
                    for value, label in zip(options, labels):
                        ET.SubElement(row, "option", value=value, label=label or value)
    issues = ET.SubElement(config, "issues")
    if issues_path:
        titles = {schema["id"]: schema.get("title") or schema["id"]
                  for schema in (json.loads(path.read_text(encoding="utf-8")) for path in schema_paths)}
        reports = json.loads(issues_path.read_text(encoding="utf-8"))
        for report in sorted(reports, key=lambda report: report["severity"] != "ERROR"):
            issue = ET.SubElement(issues, "issue", mod=report["modId"], id=report["id"],
                                  modTitle=titles.get(report["modId"], report["modId"]), severity=report["severity"])
            for field in ("title", "impact", "nextSteps"):
                ET.SubElement(issue, field).text = report.get(field, "")
    ET.indent(config)
    ET.ElementTree(config).write(output / "preview.xml", encoding="utf-8", xml_declaration=True)


def float_slider(minimum, maximum, step):
    # Match the native FloatSlider coordinates used by the same menu SWF.
    result = {"editable": "false", "decimals": "-1"}
    if minimum is None or maximum is None or minimum >= maximum:
        return result
    minimum, maximum, step = float(minimum), float(maximum), float(step)
    magnitude = max(abs(minimum), abs(maximum))
    if step < math.nextafter(magnitude, math.inf) - magnitude:
        return result
    safe_integer = 2**53 - 1
    for decimals in range(10):
        scale = 10**decimals
        coordinates = []
        for value in (minimum, maximum, step):
            scaled = value * scale
            if not math.isfinite(scaled) or abs(scaled) > safe_integer:
                break
            integer = round(scaled)
            if integer / scale != value:
                break
            coordinates.append(integer)
        if len(coordinates) != 3 or coordinates[2] <= 0:
            continue
        low, high, increment = coordinates
        span = high - low
        count = (span - 1) // increment + 1
        if span > safe_integer or count > 2**32 - 1:
            return result
        return {"editable": "true", "decimals": str(decimals), "sliderMinimum": str(low),
                "sliderMaximum": str(high), "sliderStep": str(increment), "sliderScale": str(scale),
                "sliderSteps": str(count)}
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--large", action="store_true")
    parser.add_argument("--menu", type=Path, required=True)
    parser.add_argument("--issues", type=Path)
    parser.add_argument("schemas", type=Path, nargs="+")
    args = parser.parse_args()
    prepare(args.archive, args.output, args.schemas, args.large, args.menu, args.issues)
