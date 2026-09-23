# Localization

Translate any mod's settings page by dropping in a JSON catalog. The original schema and plugin need no changes, so translators can ship catalogs as a separate mod.

OSF uses Starfield's language (`sLanguage` in `[General]`), read at startup. Restart the game after changing the language or editing catalogs.

## Catalogs

Put UTF-8 JSON files at:

```text
Data/SFSE/Plugins/OSF/Settings/translations/<language>/<modId>.json
```

`<modId>` is the schema filename without `.json`. `<language>` is Starfield's language code: `en`, `de`, `es`, `fr`, `it`, `ja`, `pl`, `ptbr` or `zhhans`. Codes from language mods also work.

```json
{
  "version": 1,
  "title": "Mein Mod",
  "description": "Beschreibung des Mods.",
  "groups": { "General": { "label": "Allgemein" } },
  "settings": {
    "enabled": { "label": "Aktivieren", "hint": "Diese Funktion einschalten." },
    "mode": { "optionLabels": { "quiet": "Leise", "normal": "Normal" } }
  },
  "hotkeys": { "open": { "label": "Einstellungen öffnen" } },
  "actions": {
    "reset": { "label": "Zurücksetzen", "hint": "Den Zustand zurücksetzen.", "confirmation": "Wirklich zurücksetzen?" }
  }
}
```

- Entries are keyed by the schema's IDs: group names, setting keys, hotkey and action IDs, and enum option values. They are exact and case-sensitive. A group OSF creates automatically is `General`.
- Actions go in `actions` no matter which group they appear in.
- Labels must be nonempty and single-line. Hints, descriptions and confirmations may contain `\n`. A confirmation can only be translated if the schema has one, and it can't be empty.

An invalid entry falls back on its own. Invalid JSON or duplicate keys reject the whole file. Errors are logged to `OSF Settings.log` with the file and field.

Translated text also appears in the registry API and vanilla Controls. IDs and stored values never change.

See the complete example: a [schema](../examples/localization/schemas/localization-example.json) and its [German catalog](../examples/localization/translations/de/localization-example.json).
