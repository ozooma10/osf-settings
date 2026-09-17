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
6. Leave four sample reports active: an error, a warning with details, a title-only
   warning, and a warning with long text for scrolling.

Each operation logs its returned status. The final four reports remain active for
the game process so the Mod Issues menu can be inspected. Their titles start with
`Example:` and do not describe actual problems. Real mods should report
actual detected problems and clear them after confirming recovery. See the
[API contract](../../docs/DIAGNOSTICS.md) for ownership and lifecycle details.

## Build

From the repository root, using its configured XMake mode:

```powershell
xmake build osfsettings-diagnostics-example
```

For the releasedbg configuration, the output is
`build/windows/x64/releasedbg/OSFSettingsDiagnosticsExample.dll`.
The target is excluded from default builds, does not auto-install, and has no
install/package files, even when using `--all`. It is not included in the
OSF Settings player package.

## Manual game check

1. Copy the example DLL into `SFSE/Plugins` in an enabled development MO2 mod
   alongside an OSF Settings build providing the diagnostics API. For a temporary
   local check it can also be copied into the enabled `OSF Settings Slim` mod;
   remove that example DLL when finished.
2. Start Starfield through SFSE and inspect `OSFSettingsDiagnosticsExample.log`
   in the normal SFSE log directory. Expect an API-acquired line and all five
   lifecycle operations plus the four `Show sample` operations ending in `Ok`.
3. Open Settings and select **Mod Issues**. Expect four example reports (plus
   any reports from other mods), the error before the warnings, no empty detail
   headings for the title-only report, and reachable `END OF LONG REPORT` text
   in the long report. Try both normal and large text.
4. To check the optional dependency, repeat with OSF Settings disabled. Expect
   `Mod Issues API unavailable; skipping the development example`.
5. Disable the example mod (or remove/rename `OSFSettingsDiagnosticsExample.dll`)
   and restart the game to test the empty state when no other mods report issues.
   SFSE plugins are loaded at process startup; reopening Settings is not enough.

The log checks cross-plugin API acquisition and returned statuses. Visual layout,
input and scrolling still require the manual menu checks above. This startup
example does not exercise live report updates or clearing while the menu is open.
