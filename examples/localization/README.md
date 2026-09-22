# Drop-in localization example

Copy `schemas/localization-example.json` to
`Data/SFSE/Plugins/OSF/Settings/schemas/`. Copy `l10n/de/localization-example.json`
to `Data/SFSE/Plugins/OSF/Settings/l10n/de/` in a separate translation mod.
Restart Starfield with German selected. No schema edits are required to enable
the translated title, description, group, setting text, options, hotkey, or action.

The schema is an authoring example. Its action has no registered handler and
remains unavailable until a consumer registers `reset`. The hotkey declares a
menu name that the consuming mod must supply. These files are not packaged with
OSF's production release.

See [Localization](../../docs/LOCALIZATION.md) for fallbacks and format rules.
