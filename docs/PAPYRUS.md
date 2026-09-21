# Papyrus settings

Compile against [OSFSettings.psc](../data/Scripts/Source/OSFSettings.psc).
OSF Settings supplies the compiled PEX; no framework ESM is needed.
[Declare your settings](SETTINGS.md), then read them from your script:

```papyrus
If OSFSettings.IsReady()
    Bool enabled = OSFSettings.GetBool("mymod", "enabled", false)
    Bool saved = OSFSettings.SetBool("mymod", "enabled", !enabled)
EndIf
```

- Reads: `GetBool`, `GetInt`, `GetFloat`, `GetEnum`, `GetString`. Each takes
  `(modId, key, fallback)` and returns the fallback on failure.
- Writes: matching `Set*` functions take `(modId, key, value)`.
  `Reset(modId, key)` and `ResetMod(modId)` restore defaults.
- Writes/resets return `true` when saved; failure leaves the old value unchanged.
  Values are shared across save games. Failures are logged in `OSFSettings.log`.
- IDs, keys, and enum options use the schema's exact spelling. Enums and strings
  are distinct types. There are no Papyrus `GetKey` / `SetKey` functions.
- Papyrus integers are 32-bit and floats are single precision. Reads that overflow
  return the fallback; float precision may be reduced.

For live changes, register before reading. Use a bound quest, reference, or alias
script and implement this function with exactly two String parameters:

```papyrus
Bool registered = OSFSettings.RegisterForChanges(Self, "mymod")

Function OnOSFSettingChanged(String modId, String key)
    ; Reread the values your mod uses.
    ; Empty key means refresh all, including the initial notification.
EndFunction
```

Register from `OnInit` and again from a player alias's `OnPlayerLoadGame`.
Registrations are cleared on load or return to the main menu. Repeating a
registration succeeds without adding duplicates. Keep the receiving script alive.
Notifications may coalesce; reread current values rather than counting changes.

For a [hotkey](Keybindings.md) declared without `menu`:

```papyrus
Bool registered = OSFSettings.RegisterHotkey(Self, "mymod", "toggle")

Function OnOSFHotkey(String modId, String hotkeyId)
    ; Run your action once per accepted key-down.
EndFunction
```

Global scripts use `RegisterForChangesStatic("MyScript", "mymod")` and
`RegisterHotkeyStatic("MyScript", "mymod", "toggle")`, with the same callbacks
marked `Global`. An owning quest/alias must register them after each load.
Papyrus schedules callbacks; delivery is not synchronous.

See the [instance and Global example](../examples/papyrus/README.md) for a complete
consumer, including initialization and load events.

For menu buttons, use [`RegisterAction` / `RegisterActionStatic` and
`CompleteAction`](ACTIONS.md#papyrus). Each action has one handler and an opaque
invocation token; closing Settings does not invalidate pending completion.
