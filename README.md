# OSF Settings

A shared Starfield menu for mod settings, interface launchers, keybindings, and reported issues.

For mod authors, start with [a settings schema](docs/SETTINGS.md), then use C++
or Papyrus to read values and respond to changes.

| Guide | Use it to |
| --- | --- |
| [Settings schemas](docs/SETTINGS.md) | Add your mod's settings to the menu |
| [C++ settings](docs/API.md) | Read, write, and watch settings from an SFSE plugin |
| [Papyrus settings](docs/PAPYRUS.md) | Read, write, and watch settings from scripts |
| [Hotkeys](docs/Keybindings.md) | Register rebindable actions or open a native menu |
| [Action buttons](docs/ACTIONS.md) | Run a confirmed operation and report completion |
| [Menu launchers](docs/LAUNCHERS.md) | Add native menus or provider-owned interfaces to the Launcher tab |
| [Settings registry](docs/REGISTRY.md) | Discover mods, metadata, and current values |
| [Issue reporting](docs/DIAGNOSTICS.md) | Report and clear problems shown in Mod Issues |

The [quickstart](docs/QUICKSTART.md) walks through a first settings page.

## Release package

```powershell
pwsh -NoProfile -File packaging/build-archive.ps1
```

Builds a production ZIP, checksum and manifest in isolated staging under
`build/packages`. See [packaging](docs/PACKAGING.md) for contents and release checks.

## Build and test

Use XMake 3.0.0+ and an MSVC compiler with C++23 support. Run from this repository:

```powershell
pwsh tools/setup-scaleform.ps1
xmake f -y -m releasedbg --test_harness=n
xmake build
```

Build and run all standalone test suites:

```powershell
xmake test
```

Test targets live in [tests/xmake.lua](tests/xmake.lua) and are excluded from normal
builds. To run one suite, use `xmake test osfsettings-registration-tests/default`.
