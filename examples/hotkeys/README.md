# Hotkey example

Registers a callback at SFSE `kPostPostLoad`. Each accepted F6 press toggles an
atomic flag and logs its value. See the [hotkey guide](../../docs/Keybindings.md).

Build from the repository root with the Windows/MSVC toolchain:

```powershell
xmake build osfsettings-hotkeys-example
```

For testing, copy `OSFSettingsHotkeysExample.dll` to `Data/SFSE/Plugins` and
[the schema](osfsettings-hotkeys-example.json) to
`Data/SFSE/Plugins/OSF/Settings/schemas`. Restart Starfield, then rebind the action
under **Hotkey callback example → General**.

The example is excluded from normal builds and installation. Its callback owner
lives until process exit; hotkey callbacks have no unregister operation.
