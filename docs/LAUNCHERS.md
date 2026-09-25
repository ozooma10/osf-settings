# Menu launchers

Add launch cards for your menus to the **Home** tab, above the mod settings list. Declare native menus in your schema, or register a C++ callback to open custom interfaces such as ImGui or WebView overlays.

## Schema

Add a `menus` object keyed by ID to `Data/SFSE/Plugins/OSF/Settings/schemas/mymod.json`:

```json
{
  "title": "My Mod",
  "groups": {},
  "menus": {
    "editor": { "title": "Open editor", "menu": "MyModMenu", "description": "Optional." }
  }
}
```

- Each key is the card's `id`: nonempty, single-line, case-sensitive and unique within the mod.
- `title` and `menu` are required. `description` is optional.
- `menu` is the exact, case-sensitive name your mod registers with `RE::UI`. If the menu isn't registered, the card shows as unavailable.

[`examples/launchers/absolute-control.json`](../examples/launchers/absolute-control.json) adds Absolute Control's `AbsoluteControlPanelMenu`.

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

// During SFSE kPostLoad:
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
- `modId` follows the [schema ID rules](SETTINGS.md#schema-fields-and-types) and is limited to 128 bytes. `id` must be nonempty, single-line UTF-8, up to 256 bytes. Titles and menu names are limited to 256 bytes; descriptions and reasons to 4096.

## Opening

- An accepted launch closes the Pause menu if open, then closes Settings and hands off to your interface.
- Your `open` callback must return promptly and queue the open on your usual UI or runtime thread.
- A rejected request leaves Settings open. You handle and report any failure after handoff (see [Issue reporting](DIAGNOSTICS.md)).
- Handoff is canceled if a main menu or loading screen is already open when Settings closes.

