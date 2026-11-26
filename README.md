# OSF Settings

A shared Starfield menu for mod settings, interface launchers, keybindings, and reported issues.

For mod authors, start with [Settings](docs/SETTINGS.md) to define a page and connect it to C++ or Papyrus.

Version 1.2.0 adds [shared pages for add-ons](docs/SETTINGS.md#extend-another-mods-page),
[dependency checks](docs/SETTINGS.md#versions-and-dependencies), clearer issue
details, and launchers on mod pages. See the [release notes](packaging/RELEASE_NOTES.txt)
and [installation guide](packaging/README.txt).

| Guide | Use it to |
| --- | --- |
| [Settings](docs/SETTINGS.md) | Define settings, action buttons and placement editors, read and write values, and respond to changes |
| [Hotkeys](docs/KEYBINDINGS.md) | Register rebindable actions or open a menu |
| [Issue reporting](docs/DIAGNOSTICS.md) | Report and clear problems shown in Mod Issues |

<details>
<summary>additional integrations</summary>

- [Menu launchers](docs/LAUNCHERS.md): add native menus or custom interfaces to the launch cards listing.
- [Localization](docs/LOCALIZATION.md): Translate an existing mod with drop-in catalogs
- [Runtime providers](docs/PROVIDERS.md): register settings with caller-owned persistence and observe key settings.

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
Packaging requires a clean, committed checkout. Use the
[release validation guide](docs/RELEASE_VALIDATION.md) and
[1.2.0 checklist](RELEASE_CHECKLIST.md) to validate a candidate before publishing.
