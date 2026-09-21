# Issue reporting example

Reports, updates, and clears issues at SFSE `kPostPostLoad`, then leaves four
sample reports for inspecting the Mod Issues tab. It needs no settings schema.
See the [issue reporting guide](../../docs/DIAGNOSTICS.md).

Build from the repository root with the Windows/MSVC toolchain:

```powershell
xmake build osfsettings-diagnostics-example
```

For testing, copy `OSFSettingsDiagnosticsExample.dll` to `Data/SFSE/Plugins`
alongside OSF Settings. Check `OSFSettingsDiagnosticsExample.log` for returned
statuses and open **Mod Issues** to view the samples. The reports are fictional.
Remove the example DLL and restart to remove them.

The example is excluded from normal builds and installation.
