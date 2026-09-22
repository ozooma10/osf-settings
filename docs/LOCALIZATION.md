# Localization

OSF follows Starfield's current `sLanguage:General` setting at startup. Restart
the game after changing its language or installing/editing catalogs. There is no
OSF language override or live reload. English interface text is included, with
embedded English defaults if the installed catalog is missing or damaged.

## Drop-in translations

Install UTF-8 JSON files under:

```text
Data/SFSE/Plugins/OSF/Settings/l10n/<language>/<modId>.json
```

Use Starfield's language identifiers: `en`, `de`, `es`, `fr`, `it`, `ja`, `pl`,
`ptbr`, and `zhhans`. The game setting is matched without regard to case. Other
safe language identifiers also work when supplied by a game language mod.
OSF follows the engine setting, so a language patch that retains `en` must
provide an `en` catalog. OSF does not infer languages from translated text.

Translations target the original schema's IDs, not its displayed labels. A
translator can publish a separate mod containing only these files; the original
schema and plugin need no changes. Normal mod-manager file priority chooses the
winning file when two packages provide the same path. OSF does not merge hidden
copies of an overridden file.

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
    "reset": {
      "label": "Zurücksetzen",
      "hint": "Den Zustand zurücksetzen.",
      "confirmation": "Wirklich zurücksetzen?"
    }
  }
}
```

Only `version: 1` is required. Omitted fields resolve individually from the
selected language, then an optional `en` catalog, then the original schema.
Setting keys, group IDs, action/hotkey IDs, and enum option values are exact and
case-sensitive, including punctuation. Enum labels are keyed by stored value,
not option position. An automatically created group has the ID `General`.

Labels must be nonempty single-line text. Descriptions and hints may be empty
and may contain JSON `\n` line breaks. Confirmation text may contain line breaks
but cannot be empty; it only translates a confirmation already present in the
schema. Translations cannot add/remove a confirmation, change values/defaults,
alter validation, or move settings to another group. NUL and other unsupported
control characters are rejected.

Missing catalogs are normal. Invalid entries fall back while valid siblings
remain usable. Invalid JSON, duplicate keys, and unsupported versions reject the
whole file. Diagnostics appear in `OSF Settings.log` with the file and field.
Catalogs for uninstalled mods are ignored.

The menu, registry API, native Controls list, and native binding conflict labels
share translated schema metadata. Registry identities and typed values stay
unchanged. A consumer that reads before language initialization receives a
normal full-refresh notification after localized metadata becomes available.

## Translating OSF itself

Copy the shipped `l10n/en/osfsettings.json` to another language directory. Its
`ui` object contains OSF's menu, buttons, help, validation, status, keyboard
captions, and other player-facing messages. The `ui` section is reserved for
the `osfsettings` catalog. Its schema fields also translate OSF's own hotkey.

Keep message keys stable. Preserve named placeholders such as `{count}`,
`{mod}`, and `{action}`; they may be reordered or repeated. Replacement text is
literal and is never interpreted as another template. Missing, extra, or broken
placeholders make that entry fall back. Count messages use count-neutral labels
so they do not impose English singular/plural rules.

Font rendering uses Starfield's active font configuration and shared fonts.
Check translations in both normal and large-text modes, including longer labels
and the actual language's glyphs. Logs and canonical key/event names are not
localized. Custom issue descriptions and action-completion messages supplied at
runtime remain the reporting mod's responsibility.

See [the complete example](../examples/localization/README.md).

## Development checks

`osfsettings-localization-tests` covers catalog validation, per-field fallback,
Unicode, formatting, and stable schema identities. The registry tests also
check localized snapshots, full-refresh notifications, and unchanged enum values.

With an instrumented Settings build staged in the sibling OSF Test Harness,
run `pwsh -File tests/harness/Test-Localization.ps1`, then repeat with `-LargeText`.
The scenario uses fresh English, German, and Japanese sessions in the harness's
private profile, installs temporary catalogs separately from the example schema,
and checks language detection, native/GFx labels, enum saves, and menu reopening.
It captures both pages for visual inspection and restores the temporary catalogs
and profile settings. The German/Japanese interface snippets are test fixtures,
not shipped translations. Return the build configuration to `--test_harness=n`
after staging test binaries.
