# Menu launchers

The **Home** tab shows recent native menus and callback-owned interfaces above
the mod settings list. Declare native menus in the mod's existing settings schema;
SDK providers can register directly. Launchers have no stored values, are never
reset, and do not add entries to the mod settings list or settings groups.

Each destination has a card with a circular vector badge, generated initials,
a stable accent color derived from its mod/destination ID, a title, a short
description, and an Open footer. This presentation needs no external images.
The current registration contract does not accept custom icon files.

Cards sort by last opened, with never-opened destinations ordered by mod title,
interface title, and stable IDs. Normal text has five shelf slots; large text has
four. When there are more interfaces than slots, the final card becomes **Show X
more** (ten interfaces show four recent cards and **Show 6 more** at normal size).
Selecting it expands the grid on Home. **Show less** or Back restores the shelf
and settings list. Expanded grids hold ten cards per page, or eight at large text.

Use Up from the first mod to focus the shelf, and Down from the shelf to return
to mod settings. Cards support directional keyboard/controller navigation, click,
and the Open prompt. In the expanded grid, mouse wheel, Page Up/Down, and visible
Prev/Next controls move between pages; directional navigation also reveals the
selected page. Shoulder buttons switch the top-level tabs. Unavailable destinations
remain visible with their reason in the footer and cannot be opened. With no
registered interfaces, Home uses the full-height settings list.

Recency is recorded when Settings hands off an accepted open request, and saved
to `recentLaunchers` in `Data/SFSE/Plugins/OSF/Settings/internal.json`. It survives menu and game
restarts and is shared across saves. Canceled or rejected requests do not update
it. See [Persistence](PERSISTENCE.md). Providers own failures after handoff. Invalid history falls back to title order;
an unwritable history still updates the current session and never blocks opening.

## Registered native menus

Add a top-level `menus` array to `Data/SFSE/Plugins/OSF/Settings/schemas/mymod.json`:

```json
{
  "schemaVersion": 1,
  "title": "My Mod",
  "groups": {},
  "menus": [
    { "id": "editor", "title": "Open editor", "menu": "MyModMenu" }
  ]
}
```

The filename without `.json` is the OSF mod ID (`mymod` here). `title` defaults to it.
Each menu requires its own stable
`id`, display `title`, and registered native `menu` name; `description` is optional.
Native IDs are exact and case-sensitive. `menus` is optional and uses the existing
schemaVersion 1 loader, validation, and load-error reporting. Keep normal settings
in `groups`; use `"groups": {}` for a menu-only mod. No separate menus file or
directory is read.

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
copied metadata/callback identity, availability updates, ABI discovery, persisted
recency, and registration from loaded schemas. `xmake test osfsettings-schema-tests/default`
covers menu declarations and their separation from saved setting values. Build the production DLL and normal/large
movies with `xmake build "OSF Settings"`.

`pwsh tools/test-menu-preview.ps1` (also with `-LargeText`) checks the combined
Home layout, overflow, focus transitions, recency refresh, and empty-launcher fallback.

Fresh-game acceptance remains necessary for native-menu and OSF UI handoffs:
keyboard, mouse and controller activation; releasing the activation input;
closing and reopening Settings; unavailable/failed openings; repeated cycles;
and load/main-menu transitions. Compilation and host checks do not prove these
engine interactions.
