# OSF Settings

A Mod Settings Menu for strarfield.

## Build and test

Use XMake 3.0.0+ and an MSVC compiler with C++23 support. Run from this repository:

```powershell
pwsh tools/setup-scaleform.ps1
xmake f -y -m releasedbg
xmake build
xmake install "OSF Settings"
```

With `XSE_SF_MODS_PATH` set, this checkout installs into the `OSF Settings Slim`
MO2 mod folder. The build target and DLL remain `OSF Settings` and `OSFSettings.dll`.
Run the install step after building to deploy SWF-only changes too; CommonLibSF's
automatic install is triggered by changes to the native DLL.

The [native hotkey callback API](docs/API.md#native-hotkey-callbacks) uses schema
hotkeys without a `menu` target. A buildable consumer lives in
[examples/hotkeys](examples/hotkeys/README.md).

The [registry snapshot API](docs/API.md#registry-discovery-and-snapshots) discovers
mods, schema metadata, and current typed values. A buildable consumer in
[examples/registry](examples/registry/README.md) refreshes owned values after
initial, keyed, and full-refresh notifications. Focused checks:

```powershell
xmake build osfsettings-registry-tests
xmake run osfsettings-registry-tests
xmake build osfsettings-registry-example
```

Focused hotkey checks on Windows:

```powershell
xmake run osfsettings-schema-tests
xmake run osfsettings-registration-tests
xmake run osfsettings-input-tests
xmake run osfsettings-lifecycle-tests
xmake run osfsettings-binding-tests
xmake run osfsettings-bindings-menu-tests
xmake run osfsettings-binding-snapshot-tests
xmake run osfsettings-hotkey-block-tests
xmake run osfsettings-hotkey-callback-tests
```

The callback state tests also run without the Windows/game dependencies, using
a test-only SFSE task stub:

```sh
clang++ -std=c++23 -Wall -Wextra -Wpedantic -Werror -pthread -Itests/stubs -Isrc \
  tests/hotkey_callback_tests.cpp src/Input/HotkeyInputState.cpp \
  -o /tmp/osfsettings-hotkey-callback-tests
/tmp/osfsettings-hotkey-callback-tests
```

Free-form string settings use a bounded UTF-8 contract and an editable Scaleform
field. See [strings in the C++ API](docs/API.md#free-form-strings). Focused checks:

```powershell
xmake build osfsettings-string-tests
xmake run osfsettings-string-tests
```

## Menu design and offline preview

Preview the layout with development-only sample settings:

```powershell
pwsh tools/preview-menu.ps1 -Design -Watch
pwsh tools/test-menu-preview.ps1 -Bindings
pwsh tools/test-menu-preview.ps1 -Bindings -LargeText
```

The root **KEYBINDINGS** page displays native MainGameplay keyboard/mouse actions
and registered OSF hotkeys. See [Keybindings](docs/Keybindings.md) for its data
ownership, interaction contract, and verification boundaries.

## In-game testing

Development-only instrumentation lives in [tests/harness](tests/harness/README.md).
It is disabled by default (`--test_harness=n`). That folder documents test builds,
isolated settings storage, and the observation interface used by the sibling
`OSF Test Harness` project's `SettingsSmoke` scenario.

Run its `Test-Starfield.ps1 -Scenario SettingsSmoke -Keybindings` for the native
page checks. It backs up global Controls, exercises native saves and a fresh
restart, then stops its game and restores the exact original Controls file.
