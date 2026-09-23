# Menu launchers

Add launch cards for your menus to the **Home** tab, above the mod settings list. Declare native menus in your schema, or register a callback in C++ for supporting imgui/webviews.

## Schema

Add a `menus` object keyed by ID to `Data/SFSE/Plugins/OSF/Settings/schemas/mymod.json`:

```json
{
  "schemaVersion": 1,
  "title": "My Mod",
  "groups": {},
  "menus": {
    "editor": { "title": "Open editor", "menu": "MyModMenu", "description": "Optional." }
  }
}
```

- Each key is the card's `id`: nonempty, single-line, case-sensitive and unique within the mod. Don't repeat `id` inside the entry. The array form is no longer accepted.
- `title` and `menu` are required. `description` is optional.
- `menu` is the exact, case-sensitive name your mod registers with `RE::UI`. It is not a SWF path. If the menu isn't registered, the card shows as unavailable.
- Use `"groups": {}` for a menu-only mod.

[`examples/launchers/absolute-control.json`](../examples/launchers/absolute-control.json) adds Absolute Control's `AbsoluteControlPanelMenu`. Install it as `schemas/absolute-control.json` only alongside that mod. If the mod already ships that schema, merge the `menus` object into it.

## C++

Copy [`sdk/OSFSettings_Launcher.h`](../sdk/OSFSettings_Launcher.h). There is nothing to link.

```cpp
#include "OSFSettings_Launcher.h"
using namespace OSFSettings::API::Launcher;

void Open(const char* modId, const char* id, void* context) noexcept
{
    // Queue your normal open request and return. Copy any strings you need later.
}

Client launcher;

// During SFSE kPostPostLoad:
if (launcher.Init()) {
    auto status = launcher.Register({
        .modId = "mymod", .id = "editor", .modTitle = "My Mod",
        .title = "Open editor", .open = Open
    });
    // Check status == Status::Ok.
}

// Disable or re-enable the card later:
launcher.SetAvailable("mymod", "editor", false, "Requires My Mod Assets.");
```

- Set exactly one of `menu` (a native menu name) or `open` (a callback). `modTitle` defaults to `modId`. `description` is optional.
- Metadata is copied. `open` and `context` must stay valid until the process exits.
- Registering the same `(modId, id)` twice returns `AlreadyRegistered`.
- An unavailable card stays visible, shows `reason`, and cannot be opened.
- `modId` follows the [schema ID rules](SETTINGS.md) and is limited to 128 bytes. `id` must be nonempty, single-line UTF-8, up to 256 bytes. Titles and menu names are limited to 256 bytes; descriptions and reasons to 4096.
- Registry calls are thread-safe. Callbacks run outside registry locks.

## Opening

- Settings waits for the activating button to be released (or for the mouse click), closes, and then opens the destination from `OnRemovedFromMenuStack`. Native menus open with a `UIMessageQueue` Show.
- Your `open` callback must return promptly and queue the open on your usual UI or runtime thread. OSF UI interfaces use their normal `RequestMenu` path.
- After handoff, your interface owns its own lifetime. Settings doesn't reopen afterward; players reopen it the usual way.
- A rejected request leaves Settings open. You handle and report any failure after handoff (see [Issue reporting](DIAGNOSTICS.md)).
- Handoff is canceled if a main menu or loading screen is already open when Settings closes.

## Display

- Cards are sorted by last opened. Cards that have never been opened follow, sorted by mod title and then card title.
- Recency counts only opens that succeed. It is shared across saves and stored as `recentLaunchers` in `Data/SFSE/Plugins/OSF/Settings/internal.json`.
- The shelf shows up to 5 cards, or 4 with large text. If there are more, the last slot becomes **Show N more**.
- Each card shows generated initials and an accent color derived from its IDs. Custom icons aren't supported.
