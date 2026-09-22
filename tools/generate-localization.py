"""Generate native and movie English fallbacks from the shipped catalog."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parent.parent
source = root / "data/SFSE/Plugins/OSF/Settings/translations/en/osfsettings.json"
catalog = json.loads(source.read_text(encoding="utf-8"))
assert catalog["version"] == 1 and all(isinstance(v, str) for v in catalog["ui"].values())
for key, value in catalog["ui"].items():
    remaining = re.sub(r"\{[A-Za-z0-9_]+\}", "", value)
    if "{" in remaining or "}" in remaining or not value or "\0" in value:
        raise ValueError(f"Invalid English message: {key}")
for path in [*root.glob("src/**/*.cpp"), *root.glob("scaleform/src/*.as")]:
    for key in re.findall(r'\b(?:Localization(?:::Text|\.text)|tr)\("([^"\n]+)"', path.read_text(encoding="utf-8")):
        if key not in catalog["ui"]:
            raise ValueError(f"Missing English message {key} in {path.relative_to(root)}")
output = root / "build/generated"
output.mkdir(parents=True, exist_ok=True)
files = {
    "English.h": '#pragma once\nnamespace OSFSettings::Localization { inline constexpr char English[] = R"OSFL10N(' + json.dumps(catalog, ensure_ascii=True) + ')OSFL10N"; }\n',
    "English.as": 'package { public final class English { public static const ui:Object = ' + json.dumps(catalog["ui"], ensure_ascii=True) + '; } }\n',
}
for name, text in files.items():
    path = output / name
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8")
