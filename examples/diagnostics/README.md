# Mod Issues native example

This development-only SFSE plugin consumes the public
[diagnostics SDK](../../sdk/OSFSettings_Diagnostics.h). It uses the mod ID
`osfsettings-diagnostics-example` without registering a settings schema.

At SFSE `kPostPostLoad`, it acquires the API and runs this sequence:

1. Report a warning with optional impact and next-step text.
2. Update the same issue to an error, omitting those optional fields to clear them.
3. Report a second issue under the same mod ID.
4. Clear the first issue by ID.
5. Clear the remaining issues belonging to the example mod.

Each operation logs its returned status. Successful completion leaves no example
issues active. This is a short startup demonstration; real mods should report
actual detected problems and clear them after confirming recovery. See the
[API contract](../../docs/DIAGNOSTICS.md) for ownership and lifecycle details.

## Build

From the repository root, using its configured XMake mode:

```powershell
xmake build osfsettings-diagnostics-example
```

For the debug configuration, the output is
`build/windows/x64/debug/OSFSettingsDiagnosticsExample.dll`.
The target is excluded from default builds, does not auto-install, and has no
install/package files, even when using `--all`. It is not included in the
OSF Settings player package.

## Manual game check

1. Copy the example DLL into `SFSE/Plugins` in a separate development MO2 mod.
   Enable it alongside an OSF Settings build providing the diagnostics API.
2. Start Starfield through SFSE and inspect `OSFSettingsDiagnosticsExample.log`
   in the normal SFSE log directory. Expect an API-acquired line and all five
   operation labels above ending in `Ok`.
3. To check the optional dependency, repeat with OSF Settings disabled. Expect
   `Mod Issues API unavailable; skipping the development example`.
4. Disable the example mod when finished.

The log checks cross-plugin API acquisition and returned statuses. It does not
inspect the registry or prove menu behavior. The example clears its reports
during startup, and the Mod Issues menu remains a later checkpoint.
