# Settings schemas

Ship `Data/SFSE/Plugins/OSF/Settings/schemas/mymod.json` with your mod:

```json
{
  "schemaVersion": 1,
  "id": "mymod",
  "title": "My mod",
  "groups": {
    "General": [
      {
        "key": "enabled",
        "type": "bool",
        "label": "Enable feature",
        "default": true
      }
    ]
  }
}
```

OSF builds the menu from this file. Your mod reads the values and applies them
through [C++](API.md) or [Papyrus](PAPYRUS.md).

Display text can be translated with separate [localization catalogs](LOCALIZATION.md)
without modifying this schema. IDs and stored values remain unchanged.

- Required root fields: `schemaVersion: 1`, `id`, and `groups`. `title` defaults
  to the mod ID; `description` is optional.
- Mod IDs use lowercase ASCII letters, digits, `.`, `_`, or `-`; empty IDs,
  `.` and `..` are invalid. Use a unique ID and keep it stable between releases.
- Group names become headings. Groups and settings appear in authored order.
- Every setting needs `key`, `type`, and `default`. Keys must be nonempty and
  unique across the mod. API lookups use exact, case-sensitive keys.
- Optional `label` defaults to the key. `hint` adds help text.

| Type | Default example | Additional fields |
| --- | --- | --- |
| `bool` | `true` | None |
| `int` | `3` | Optional inclusive `min` / `max`; signed 64-bit |
| `float` | `0.75` | Optional inclusive `min` / `max`; positive `step` defaults to `0.1` |
| `enum` | `"normal"` | Required `options` array; optional matching `optionLabels` array |
| `string` | `"auto"` | `maxLength`: 1–4096 UTF-8 bytes; defaults to 256 |
| `key` | `"F4"` | `allowUnbound` defaults to false; C++ reads/writes keyboard VK codes |

Defaults must satisfy the setting's type and limits. Float `step` controls the
editor increment; writes need not be multiples of it. Enum options are unique,
nonempty strings; the default and API values must match an option exactly.

```json
{
  "key": "mode",
  "type": "enum",
  "default": "normal",
  "options": ["quiet", "normal", "verbose"],
  "optionLabels": ["Quiet", "Normal", "Verbose"]
}
```

Strings are single-line UTF-8. Empty text is allowed; NUL, control characters,
and line/paragraph separators are rejected. Limits count bytes, not characters.
Values are preserved without trimming or truncation.

A `key` setting stores a value for your own input handler. Use [hotkeys](Keybindings.md)
for OSF-dispatched actions. Key defaults accept recognized names or bindable VK
integers; `"UNBOUND"` / `255` requires `allowUnbound: true`.

Add `"requires": "restart"` to show a restart notice. This is only a notice:
values still save immediately, and your mod decides when to apply them.

Use a top-level [`actions` array](ACTIONS.md) for buttons with optional
confirmation and asynchronous completion. Actions have no stored value/default.

Schemas load at startup; restart Starfield after editing them. Player values are
saved in `Data/SFSE/Plugins/OSF/Settings/values/<modId>.json` and shared across
save games. Ship the schema; OSF manages the values file. Load failures are
reported in `OSFSettings.log`.
