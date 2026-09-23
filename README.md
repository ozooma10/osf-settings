# OSF Settings

A shared Starfield menu for mod settings, interface launchers, keybindings, and reported issues.

For mod authors, start with [Settings](docs/SETTINGS.md) to define a page and connect it to C++ or Papyrus.

| Guide | Use it to |
| --- | --- |
| [Settings](docs/SETTINGS.md) | Define settings and action buttons, read and write values, and respond to changes |
| [Hotkeys](docs/KEYBINDINGS.md) | Register rebindable actions or open a menu |
| [Issue reporting](docs/DIAGNOSTICS.md) | Report and clear problems shown in Mod Issues |

<details>
<summary>additional integrations</summary>

- [Menu launchers](docs/LAUNCHERS.md): add native menus or custom interfaces to the launch cards listing.
- [Localization](docs/LOCALIZATION.md): Translate an existing mod with drop-in catalogs

</details>

## Build and test

Use XMake 3.0.0+, Python 3.9+, and an MSVC compiler with C++23 support. Run from this repository:

```powershell
pwsh tools/setup.ps1
xmake f -y -m releasedbg --test_harness=n
xmake build
```

Build and run all standalone test suites:

```powershell
xmake test
```

## Release package

```powershell
pwsh -NoProfile -File packaging/build-archive.ps1
```

Builds a production ZIP, checksum and manifest in isolated staging under `build/packages`.
