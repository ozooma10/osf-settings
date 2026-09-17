# OSF Settings

A Mod Settings Menu for strarfield.

## Build and test

Use XMake 3.0.0+ and an MSVC compiler with C++23 support. Run from this repository:

```powershell
pwsh tools/setup-scaleform.ps1
xmake f -y -m releasedbg
xmake build
```

## Menu design and offline preview

Preview the layout with development-only sample settings:

```powershell
pwsh tools/preview-menu.ps1 -Design -Watch
```

## In-game testing

Development-only instrumentation lives in [tests/harness](tests/harness/README.md).
It is disabled by default (`--test_harness=n`). That folder documents test builds,
isolated settings storage, and the observation interface used by the sibling
`OSF Test Harness` project's `SettingsSmoke` scenario.
