# OSF Settings Slim

A small settings mod built one layer at a time. **Checkpoint 3 adds a scrolling
checkbox menu under Pause → Mod Settings.** Each edit saves before publishing
the new value. The public reader API is the next checkpoint.

The menu uses native show/hide messages and menu-stack callbacks. Pause stays
underneath Slim, and the engine manages Slim's pause flag. The only code hook
adds **Mod Settings** to Pause's action list.

Start with [the checkpoint 1 walkthrough](docs/checkpoint-1.md).
Then follow [checkpoint 2: save and reload a value](docs/checkpoint-2.md).
The current layer is [checkpoint 3: edit the value in-game](docs/checkpoint-3.md).

## Build and test

Use XMake 3.0.0+ and an MSVC compiler with C++23 support. Run from this repository:

```powershell
pwsh tools/setup-scaleform.ps1
xmake f -y -m releasedbg
xmake build osfsettings-slim-tests
xmake run osfsettings-slim-tests
xmake build "OSF Settings Slim"
```

The test executable compiles the production parser and store without CommonLibSF
or Starfield. Its generated fixtures stay under `build/tests/checkpoint1/` and
`build/tests/checkpoint2/` for
inspection. The plugin output is `build/windows/x64/releasedbg/OSFSettingsSlim.dll`.
Ordinary builds do not deploy. CommonLibSF is pinned to
`f6d36e1d2fb441a3e9b51f8a91194a184de580d9`, the inspected menu revision used by
the full project. The DLL targets Starfield 1.16.244. The movie build uses Apache
Flex 4.16.1, Temurin 8u462, and playerglobal 10.3, downloaded with SHA256 checks
by the setup script. It builds both normal and large-text movies.

## Menu design and offline preview

The menu uses mod pages and group tabs, a compact ON/OFF list, and an open
description column. Search filters settings across the current mod's groups.
Changed values get an amber diamond; reset restores the selected setting's
schema default through the same save path as a normal edit. The current setting
type remains boolean; favorites, sliders, and dropdowns are not implemented.

Preview the layout with development-only sample settings:

```powershell
pwsh tools/preview-menu.ps1 -Design -Watch
```

Omit `-Design` to use the learning schema. Add `-LargeText` for the large-text
layout or `-Scrolling` for additional scrolling fixtures. The preview reads
local vanilla assets from `Starfield - Interface.ba2`; use `-InterfaceArchive`
if your game is installed elsewhere. It requires Python 3.9+, the Flex toolchain,
and portable Ruffle (downloaded automatically if absent). Generated libraries
and preview values stay outside the install payload; values live only in memory.
Ruffle approximates Scaleform, so the final game rendering still needs checking.

Use arrows/mouse to select, E/Enter to toggle or open a mod, F to search, B to
reset, and `[` / `]` to change group. Tab/Back clears search, returns to all mods,
then closes the menu. F5 reloads the preview. The native button bar displays the
game's mapped bindings. Saving while `-Watch` is running rebuilds and restarts
the preview; without watch mode, rerun the command after editing source.

`MenuStyle.as` holds spacing, colors, and font aliases; `SettingsRow.as` draws a
row inside the vanilla scrolling list. `OSFSettingsMenu.as` handles navigation
and the native bridge. Deploy the rebuilt DLL and SWFs together: `getRows` now
sends settings with mod/group metadata instead of inserting heading rows.

To run interaction checks and render PNG captures without controlling the desktop:

```powershell
pwsh tools/test-menu-preview.ps1
pwsh tools/test-menu-preview.ps1 -LargeText
```

These exercise toggling, reset, tabs, search, empty results, mod navigation,
last-row selection, and save failure. Captures and logs go to `build/preview/`.

## Explicit staging

To inspect the install payload without changing the game or MO2:

```powershell
xmake install -o "$PWD/build/stage" "OSF Settings Slim"
```

The payload contains `SFSE/Plugins/OSFSettingsSlim.dll`,
`SFSE/Plugins/OSF/Settings/schemas/learning.json`, and
`Interface/OSFSettingsMenu.swf` / `OSFSettingsMenu_LRG.swf`
(plus build symbols). For explicit deployment on this machine:

```powershell
xmake install -o "C:/Modding/Starfield/MO2/mods/OSF Settings Slim" "OSF Settings Slim"
```

Launch through MO2 with SFSE. The full Settings plugin and its incompatible
consumers should be disabled for Slim's game tests.

Runtime schemas are read from `Data/SFSE/Plugins/OSF/Settings/schemas/`.
Saved values live beside that directory in `values/<mod-id>.json`, shared across
game saves. Defaults are loaded first, then valid saved booleans override them.
The setter creates a values file only when a value changes. Values files are
player data and are not included in the install payload.
