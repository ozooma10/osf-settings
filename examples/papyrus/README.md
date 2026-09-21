# Papyrus example

Demonstrates values, resets, change notifications, hotkeys, and action buttons on a player alias
and a Global script. See the [Papyrus guide](../../docs/PAPYRUS.md).

The instance owns a confirmed reset action. The Global script owns a separate
action with delayed completion using `Utility.WaitMenuPause`. Action ownership
is exclusive; both scripts re-register after load.

Compile from the repository root:

```powershell
pwsh tools/build-papyrus.ps1 -Examples
```

Example PEX files are written to `build/papyrus/examples` and are not installed
automatically. The helper accepts `-Compiler` and `-Imports` for alternate CK paths.

1. Put [papyrusexample.json](papyrusexample.json) in `Data/SFSE/Plugins/OSF/Settings/schemas`.
2. Put both example PEX files in `Data/Scripts`.
3. Attach `OSFSettingsExample` to a player reference alias in your running quest.
   Its `OnInit` and `OnPlayerLoadGame` register both scripts before reading values.
4. Bind **Toggle example** in Settings; it starts unbound.

No example ESM is supplied. Global-only consumers must call
`OSFSettingsExampleGlobal.InitializeSettings()` from their own initialization/load script.
