"""Create deterministic host-only performance fixtures; never touch player values."""
import json
from pathlib import Path
import shutil
import sys


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main():
    root = Path(sys.argv[1]).resolve()
    root.mkdir(parents=True, exist_ok=False)
    inventory = json.loads(Path(sys.argv[2]).read_text(encoding="utf-8-sig"))
    # Inventory order is MO2 priority order (highest first); first file wins.
    for item in inventory:
        source = Path(item["FullName"])
        relative = source.relative_to(next(p for p in source.parents if p.name == "Settings"))
        if relative.parts[0] not in {"schemas", "translations"}:
            raise ValueError(f"Not a schema or translation: {source}")
        dest = root / "installed" / relative
        if not dest.exists():
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, dest)
    for name, mods, settings in [("typical", 10, 40), ("dense", 40, 250), ("large-page", 1, 2000)]:
        for m in range(mods):
            controls, labels = [], {}
            for i in range(settings):
                key = f"setting_{i:04d}"
                control = {"key": key, "label": f"Setting number {i}", "hint": "Authored explanatory text for this setting.", "type": "bool", "default": True}
                label = {"label": f"Translated setting number {i}", "hint": "Translated explanatory text for this setting."}
                if i % 4 == 0:
                    control.update(type="enum", default="normal", options={"quiet": "Quiet", "normal": "Normal", "verbose": "Verbose"})
                    label["optionLabels"] = {"quiet": "Translated quiet", "normal": "Translated normal", "verbose": "Translated verbose"}
                controls.append(control)
                labels[key] = label
            mod = f"bench{m:03d}"
            write(root / name / "schemas" / f"{mod}.json", {"schemaVersion": 1, "title": f"Benchmark mod {m}", "groups": {"General": controls}})
            for language in ["en", "ja"]:
                write(root / name / "translations" / language / f"{mod}.json", {"version": 1, "settings": labels})
    for fixture in root.iterdir():
        (fixture / "empty").mkdir()
        (fixture / "values").mkdir()
    write(root / "protocol.json", {"installed": "Enabled Default-profile loose schemas/catalogs; no player values copied. Archives and runtime providers excluded.",
                                  "synthetic": "10x40, 40x250 and 1x2000 controls; 25% enums, remaining bools; full English and Japanese overlays; empty persisted values."})


if __name__ == "__main__":
    main()
