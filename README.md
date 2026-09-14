# OSF Settings Slim

A small settings mod built one layer at a time. **Checkpoint 2 loads saved
boolean values and saves edits before publishing them in memory.** The in-game
menu and public reader API are later checkpoints.

Start with [the checkpoint 1 walkthrough](docs/checkpoint-1.md).
Then follow [checkpoint 2: save and reload a value](docs/checkpoint-2.md).

## Build and test

Use XMake 3.0.0+ and an MSVC compiler with C++23 support. Run from this repository:

```powershell
xmake f -y -m releasedbg
xmake build osfsettings-slim-tests
xmake run osfsettings-slim-tests
xmake build "OSF Settings Slim"
```

The test executable compiles the production parser and store without CommonLibSF
or Starfield. Its generated fixtures stay under `build/tests/checkpoint1/` and
`build/tests/checkpoint2/` for
inspection. The plugin output is `build/windows/x64/releasedbg/OSFSettingsSlim.dll`.
Ordinary builds do not deploy. CommonLibSF remains at Slim's existing revision
until the menu checkpoint.

## Explicit staging

To inspect the install payload without changing the game or MO2:

```powershell
xmake install -o "$PWD/build/stage" "OSF Settings Slim"
```

The payload contains `SFSE/Plugins/OSFSettingsSlim.dll` and
`SFSE/Plugins/OSF/Settings/schemas/learning.json` (plus build symbols).
When testing in-game later, install that payload as the separate **OSF Settings
Slim** mod through MO2 and launch with SFSE. The full Settings plugin and its
incompatible consumers should be disabled for Slim's game tests.

Runtime schemas are read from `Data/SFSE/Plugins/OSF/Settings/schemas/`.
Saved values live beside that directory in `values/<mod-id>.json`, shared across
game saves. Defaults are loaded first, then valid saved booleans override them.
The setter creates a values file only when a value changes. Values files are
player data and are not included in the install payload.
