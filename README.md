# OSF Settings

A shared Starfield menu for mod settings, keybindings, and reported issues.

For mod authors, start with [a settings schema](docs/SETTINGS.md), then use C++
or Papyrus to read values and respond to changes.

| Guide | Use it to |
| --- | --- |
| [Settings schemas](docs/SETTINGS.md) | Add your mod's settings to the menu |
| [C++ settings](docs/API.md) | Read, write, and watch settings from an SFSE plugin |
| [Papyrus settings](docs/PAPYRUS.md) | Read, write, and watch settings from scripts |
| [Hotkeys](docs/Keybindings.md) | Register rebindable actions or open a native menu |
| [Settings registry](docs/REGISTRY.md) | Discover mods, metadata, and current values |
| [Issue reporting](docs/DIAGNOSTICS.md) | Report and clear problems shown in Mod Issues |

## Build and test

Use XMake 3.0.0+ and an MSVC compiler with C++23 support. Run from this repository:

```powershell
pwsh tools/setup-scaleform.ps1
xmake f -y -m releasedbg
xmake build
```

Build and run all standalone test suites:

```powershell
xmake test
```

Test targets live in [tests/xmake.lua](tests/xmake.lua) and are excluded from normal
builds. To run one suite, use `xmake test osfsettings-registration-tests/default`.
