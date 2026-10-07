OSF Settings - Native MCM and Keybindings

Requirements: Starfield 1.16.244.0 (Steam), matching SFSE, and Address Library
for SFSE Plugins with the 1.16.244.0 database (v21). OSF UI and WebView2 are
not required by OSF Settings. Other game versions are not verified.

Install this ZIP with MO2 or Vortex and enable it. Its root is the game's Data
directory: SFSE, Interface and Scripts belong directly under Data.
Launch through SFSE, load a save, then choose MOD SETTINGS in Pause.
The opening key defaults to unbound. Assign it in the KEYBINDINGS tab.
No ESM/ESP activation is required for the framework itself.

OSF follows the game's language at startup. English and Japanese are included.
Separate translation mods can provide UTF-8 JSON catalogs in
SFSE/Plugins/OSF/Settings/translations/<language>/<modId>.json without changing a mod's
schema. Restart after changing language or installing translations. The source
repository's docs/LOCALIZATION.md describes the format and fallback rules.

ALL MODS lists supporting mods. Settings save automatically; individual reset
restores the mod author's default. Text drafts save with Enter/SAVE and can be
cancelled. A setting marked as requiring a restart takes effect when its owning
mod applies it after restarting Starfield.

KEYBINDINGS searches native MainGameplay keyboard/mouse actions and registered
mod hotkeys, with primary/alternate slots.
It cannot discover every mod's privately implemented hotkeys. MOD ISSUES shows
settings files that failed to load and reports supplied by supporting mods; an
empty page is not a health check of every installed mod. Physical controller
operation remains unverified, and text entry requires a keyboard.

Settings persist in Documents/My Games/Starfield/OSF/Settings, shared
across saves, mod-manager profiles and game installations using that Documents
folder. Launcher history is stored in the same folder in internal.json.
Documents follows the Windows location, including OneDrive redirection.
Native hotkeys use Starfield's Controls storage separately. Keep both when
upgrading. Schemas and translations remain in Data.
Do not delete the shared OSF directory; other mods may own files there.

OSF Settings owns configuration, keybindings and shared issue reporting.
OSF UI is a separate framework for custom web interfaces. Existing OSF UI or
historical Settings integrations are not automatically compatible with this
SDK; follow the requirements of each consumer mod. Do not install two providers
of OSFSettings.dll together.

If the menu is missing, confirm SFSE loaded OSFSettings.dll and the matching
Address Library is installed. Check for conflicting older copies of the DLL
or menu SWFs. Mod pages require schemas and an integration supplied by the mod.
For a report, include the game/SFSE/mod versions, reproduction steps and the
OSF Settings/SFSE logs under Documents/My Games/Starfield/SFSE/Logs.

Source and mod author guide:
https://github.com/ozooma10/osf-settings-slim

This archive contains a production build. Acceptance fixtures, sample mods,
debug symbols and personal settings are excluded. License notices are in
SFSE/Plugins/OSF/Settings/licenses.
