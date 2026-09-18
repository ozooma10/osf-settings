#!/usr/bin/env python3
"""Export exact working-tree bytes, including dirty/untracked submodule source."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import zipfile
from datetime import datetime, timezone


def git(repo, *arguments):
    return subprocess.check_output(
        ["git", "-C", str(repo), *arguments],
        env={**os.environ, "GIT_OPTIONAL_LOCKS": "0"},
    )


def digest(data):
    return hashlib.sha256(data).hexdigest()


def validate_path(name):
    parts = PurePosixPath(name).parts
    if not parts or name.startswith("/"):
        raise ValueError(f"Not a relative source path: {name!r}")
    for part in parts:
        if (
            part in (".", "..", ".git")
            or part.endswith((" ", "."))
            or any(ord(c) < 32 or c in '<>:"\\|?*' for c in part)
            or re.fullmatch(r"(?i)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\..*)?", part)
        ):
            raise ValueError(f"Unsafe or Windows-incompatible source path: {name!r}")


def capture(root):
    files = {}
    repositories = []
    provenance = {}
    folded_paths = set()

    def visit(repo, prefix):
        # Require a real initialized repository, not Git's parent-directory fallback.
        actual = Path(git(repo, "rev-parse", "--show-toplevel").decode().strip()).resolve()
        if actual != repo.resolve():
            raise ValueError(f"Uninitialized submodule: {prefix}")
        number = len(repositories)
        record = {
            "path": prefix or ".",
            "head": git(repo, "rev-parse", "HEAD").decode().strip(),
            "status": git(repo, "status", "--porcelain=v1", "--untracked-files=all").decode(),
            "ignored": git(repo, "ls-files", "--others", "--ignored", "--exclude-standard", "--directory").decode(),
        }
        repositories.append(record)
        for label, extra in (("staged", ["--cached"]), ("unstaged", [])):
            patch = git(repo, "diff", "--binary", "--no-ext-diff", "--no-textconv", *extra)
            name = f"provenance/{number}-{label}.patch"
            provenance[name] = patch
            record[label + "_patch"] = {"path": name, "sha256": digest(patch)}

        tracked = {}
        for entry in git(repo, "ls-files", "--stage", "-z").split(b"\0"):
            if not entry:
                continue
            metadata, raw_name = entry.split(b"\t", 1)
            mode, _object, stage = metadata.decode().split()
            if stage != "0":
                raise ValueError("Resolve merge conflicts before taking a source snapshot")
            tracked[raw_name.decode()] = mode
        others = git(repo, "ls-files", "--others", "--exclude-standard", "-z").split(b"\0")
        for name in sorted(set(tracked) | {n.decode() for n in others if n}):
            relative = f"{prefix}/{name}" if prefix else name
            validate_path(relative)
            path = repo / name
            if path.is_symlink():
                raise ValueError(f"Symlink requires an explicit Windows mapping: {relative}")
            if tracked.get(name) == "160000":
                visit(path, relative)
                continue
            if not path.exists():
                if name in tracked:
                    continue  # Working-tree deletions must remain absent on Windows.
                raise ValueError(f"File disappeared during capture: {relative}")
            if not path.is_file():
                raise ValueError(f"Not a regular source file: {relative}")
            # Never silently package dependencies, generated files, or machine settings.
            top = PurePosixPath(relative).parts[0]
            if top in ("external", "build", ".xmake", ".vscode", ".vs", ".idea", ".cache") or top.startswith("vsxmake") or relative == "compile_commands.json":
                raise ValueError(f"Tracked machine/generated material needs review: {relative}")
            folded = relative.casefold()
            if folded in folded_paths:
                raise ValueError(f"Windows case-insensitive path collision: {relative}")
            folded_paths.add(folded)
            before = path.stat()
            data = path.read_bytes()
            after = path.stat()
            if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
                raise ValueError(f"Source changed during capture: {relative}")
            files[relative] = data

    visit(root, "")
    return files, repositories, provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="Output directory outside source, or under ignored build/")
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    destination = (options.output or root / "build/windows-snapshots").resolve()
    if destination.is_relative_to(root) and not destination.is_relative_to(root / "build"):
        parser.error("Use an output directory outside the repository or under build/")
    first = capture(root)
    second = capture(root)
    if first != second:
        raise SystemExit("Source or Git state changed between captures; retry after editing pauses")
    files, repositories, provenance = first
    entries = [{"path": name, "sha256": digest(data), "size": len(data)} for name, data in sorted(files.items())]
    canonical = "".join(f"{e['sha256']} {e['size']} {e['path']}\n" for e in entries).encode()
    source_id = digest(canonical)
    now = datetime.now(timezone.utc)
    snapshot_id = f"{now:%Y%m%dT%H%M%SZ}-{source_id[:16]}"
    manifest = {
        "schema": 1,
        "snapshot_id": snapshot_id,
        "source_id": source_id,
        "created_utc": now.isoformat(),
        "source_root": str(root),
        "selection": "Working-tree bytes: tracked plus nonignored untracked; recursive submodules; deleted files absent",
        "repositories": repositories,
        "files": entries,
    }
    manifest_bytes = (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode()
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / f"{snapshot_id}.zip"
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED) as bundle:
        bundle.writestr(f"{snapshot_id}/manifest.json", manifest_bytes)
        bundle.writestr(f"{snapshot_id}/manifest.sha256", digest(manifest_bytes) + "\n")
        for name, data in provenance.items():
            bundle.writestr(f"{snapshot_id}/{name}", data)
        for name, data in sorted(files.items()):
            bundle.writestr(f"{snapshot_id}/source/{name}", data)
    archive_hash = digest(archive.read_bytes())
    archive.with_suffix(".zip.sha256").write_text(f"{archive_hash}  {archive.name}\n")
    print(json.dumps({
        "snapshot_id": snapshot_id,
        "source_id": source_id,
        "manifest_sha256": digest(manifest_bytes),
        "archive": str(archive),
        "archive_sha256": archive_hash,
        "files": len(entries),
        "bytes": archive.stat().st_size,
    }, indent=2))


if __name__ == "__main__":
    main()
