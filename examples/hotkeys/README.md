# Native hotkey example

This development-only SFSE plugin registers a schema hotkey callback at `kPostPostLoad`.
Each accepted F6 press toggles an atomic feature flag and logs its new value.
The schema has no `menu` target or callback-name field.

Build on the same Windows/MSVC toolchain as OSF Settings:

```powershell
xmake build osfsettings-hotkeys-example
```

The target is excluded from default builds and installation. For manual testing,
place the built `OSFSettingsHotkeysExample.dll` under `Data/SFSE/Plugins` and copy
`osfsettings-hotkeys-example.json` to `Data/SFSE/Plugins/OSF/Settings/schemas`.
Restart Starfield with the current OSF Settings provider. The hotkey is displayed
under **Hotkey callback example → General** and can be rebound there.

Each accepted key-down submits one SFSE task, with no main-thread or cross-task
serialization guarantee. This example only updates an atomic and logs; schedule engine effects
in their required context. Holding or releasing the key does not toggle again.
Gameplay control eligibility and OSF hotkey blocks apply when the press is
accepted. A later block does not cancel an accepted press's task.

The callback code and owner must stay alive until process exit. Register once
using a process-lifetime owner; there are no hotkey tokens or unregister calls:

```cpp
auto status = settings.RegisterHotkey("mymod", "toggleFeature", OnHotkey, owner);
```

Ordinary settings subscriptions still have their own subscription tokens and
cleanup rules. See [the API contract](../../docs/API.md#native-hotkey-callbacks)
for registration errors and blocking behavior.

When in-game testing is requested, use the sibling OSF Test Harness's
`SettingsSmoke` scenario and isolated profile; host tests do not prove live input
dispatch behavior.
