# Menu launchers

The top-level **Launcher** tab lists all registered native menus and callback-owned
interfaces together. Declare native menus in the mod's existing settings schema;
SDK providers can register directly. Launchers have no stored values, are never
reset, and do not add entries to All Mods or settings groups.

Each destination has a card with a circular vector badge, generated initials,
a stable accent color derived from its mod/destination ID, a title, a short
description, and the owning mod's name. This presentation needs no external images.
The current registration contract does not accept custom icon files.

Cards sort by mod title, then interface title. Normal text shows four tall cards
per page; large text shows three larger cards per page. Use directional navigation
on keyboard/controller, click a card, or use the Open prompt. Mouse wheel,
Page Up/Down, and the visible Prev/Next controls move between card pages; directional
navigation also reveals the selected page. Shoulder buttons continue to switch
the top-level tabs. Unavailable destinations remain visible with their reason in
the footer and cannot be opened.

## Registered native menus

Add a top-level `menus` array to `Data/SFSE/Plugins/OSF/Settings/schemas/<modId>.json`:

```json
{
  "schemaVersion": 1,
  "id": "mymod",
  "title": "My Mod",
  "groups": {},
  "menus": [
    { "id": "editor", "title": "Open editor", "menu": "MyModMenu" }
  ]
}
```

`id` is the OSF mod ID. `title` defaults to it. Each menu requires its own stable
`id`, display `title`, and registered native `menu` name; `description` is optional.
Native IDs are exact and case-sensitive. `menus` is optional and uses the existing
schemaVersion 1 loader, validation, and load-error reporting. Keep normal settings
in `groups`; use `"groups": {}` for a menu-only mod. The filename must match the mod
ID. No separate menus file or directory is read.

The owner must register the menu with `RE::UI` and supply its movie/native bridge.
A SWF path is not a menu name. Missing menu registrations show an unavailable card.
The hub cannot launch itself.

`examples/launchers/absolute-control.json` is an optional compatibility declaration
for Absolute Control's `AbsoluteControlPanelMenu`. Install it as
`schemas/absolute-control.json` only alongside that mod. If the mod already ships
that schema, merge the `menus` array into it. The example is not included in the
default Settings payload and does not replace its settings or controls.

## Native provider API

Include `sdk/OSFSettings_Launcher.h`. The optional service has its own version 2.0
and export, `OSFSettings_RequestLauncherAPI`; it does not extend `ISettings`.
Version 2 replaces the experimental session/reporting contract.

```cpp
using namespace OSFSettings::API::Launcher;

void Open(const char* mod, const char* id, void* context) noexcept
{
    // Queue the provider's normal open request. Strings have callback lifetime;
    // copy them if the queued request needs them later.
}

// At kPostPostLoad:
Client launcher;
if (launcher.Init()) {
    auto result = launcher.Register({
        .modId = "mymod", .id = "editor", .modTitle = "My Mod",
        .title = "Open editor", .open = Open
    });
    // Require Status::Ok; log registration failures.
}
```

Register exactly one of `menu` or `open`. Metadata is copied; callback code and
context live until process exit. The Client wrapper itself need not persist.
Use `SetAvailable` to enable or disable an entry and explain unavailability.
Mod IDs follow the Settings grammar and are bounded to 128 bytes; destination IDs
are nonempty single-line UTF-8 up to 256 bytes. Titles/menu names allow 256 bytes;
descriptions and reasons allow 4096. Identical mod/destination IDs are rejected.
Registry calls are thread-safe; callbacks run outside registry locks.

The menu waits for the activating keyboard/controller button's release (or a
mouse click), closes Settings, then hands off from `OnRemovedFromMenuStack`.
Native destinations use `UIMessageQueue` Show. Provider callbacks return promptly
and queue opening on their existing runtime lane. OSF UI uses its normal
`RequestMenu` path, including preflight, loading, input ownership and cleanup.

Each provider owns its interface's lifetime. There are no session tokens,
lifecycle reports, timeouts, or automatic return to Settings. Players reopen
Settings through its normal hotkey or menu entry. A rejected request leaves
Settings open; errors after handoff belong to the provider's usual diagnostics.
A main-menu/loading screen already open when Settings is removed cancels handoff.

## Verification

`xmake test osfsettings-launcher-tests/default` checks registry validation,
copied metadata/callback identity, availability updates, ABI discovery, and
registration from loaded schemas. `xmake test osfsettings-schema-tests/default`
covers menu declarations and their separation from saved setting values. Build the production DLL and normal/large
movies with `xmake build "OSF Settings"`.

Fresh-game acceptance remains necessary for native-menu and OSF UI handoffs:
keyboard, mouse and controller activation; releasing the activation input;
closing and reopening Settings; unavailable/failed openings; repeated cycles;
and load/main-menu transitions. Compilation and host checks do not prove these
engine interactions.
