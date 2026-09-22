# Hotkeys

Add a top-level `hotkeys` array to your [schema](SETTINGS.md):

```json
{
  "schemaVersion": 1,
  "id": "mymod",
  "title": "My mod",
  "groups": {},
  "hotkeys": [
    { "id": "toggle", "label": "Toggle feature", "default": "F6" }
  ]
}
```

Players rebind these actions in OSF Settings or vanilla Controls → Mod Bindings.
Starfield saves bindings in `ControlMap_Custom.txt`; they are separate from
ordinary settings values. Schema changes require restarting the game.

- `id` and `label` are required. Hotkey IDs use ASCII letters, digits, `_`, or `-`
  and must be unique within the mod, ignoring case. API calls use exact casing.
- Omit `default` to start unbound. Defaults are keyboard key names, matched
  without case sensitivity. Unknown names are logged and the hotkey is skipped.
- Optional `group` names an existing schema group. Otherwise the hotkey follows
  the first group's settings, or appears in General when `groups` is empty.

## C++ callbacks

For a C++ callback, omit `menu` and register once after
[client initialization](SETTINGS.md#c-integration)
at SFSE `kPostPostLoad`:

```cpp
void OnHotkey(const char* mod, const char* id, void* context) noexcept;
auto status = settings.RegisterHotkey("mymod", "toggle", OnHotkey, nullptr);
```

Check for `Status::Ok`. Registration requires ready settings and native input;
otherwise it returns `NotReady`. Each successful registration adds a callback.
There is no unregister call: callback code and context must live until process exit.
Each accepted keyboard down submits one SFSE task; holds/releases do not repeat it.
Callbacks have no main-thread or cross-task serialization guarantee. Copy retained
strings and schedule engine effects appropriately; exceptions must not escape.

See the [buildable C++ example](../examples/hotkeys/README.md).

## Papyrus callbacks

For a declaration without `menu`, register a bound quest/reference/alias from
initialization and after each load, following the
[Papyrus settings lifecycle](SETTINGS.md#papyrus-integration):

```papyrus
Bool registered = OSFSettings.RegisterHotkey(Self, "mymod", "toggle")

Function OnOSFHotkey(String modId, String hotkeyId)
    ; Run your action once per accepted key-down.
EndFunction
```

Check the registration result. Registrations are cleared on load or return to
the main menu; repeating a registration succeeds without adding duplicates.
Keep the receiving script alive. Global scripts use
`RegisterHotkeyStatic("MyScript", "mymod", "toggle")` with the same callback
marked `Global`. An owning quest/alias must register them after each load.
Papyrus schedules callbacks; delivery is not synchronous.

See the [instance and Global example](../examples/papyrus/README.md).

## Open a native menu

To open a native menu instead, add `"menu": "MyModMenu"` to the declaration.
Your plugin must register that name with `RE::UI::RegisterMenu` before accepting
input. OSF requests the menu on key release; it does not toggle it closed.
Menu hotkeys cannot also register callbacks. Native gameplay input rules apply;
OSF dispatch currently handles keyboard input.

## Block hotkeys while capturing input

For a custom view that captures input, acquire a block before granting focus:

```cpp
OSFSettings::API::HotkeyBlock block{};
auto status = settings.AcquireHotkeyBlock(&block);
// Grant focus only on Ok. After revoking focus, including cancellation/cleanup:
// settings.ReleaseHotkeyBlock(block);
```

Call `ReleaseHotkeyBlock(block)` for every acquired token. All blocks must be
released before new presses can activate. Blocks suppress OSF-dispatched hotkeys
only; they do not cancel callbacks already queued or disable other input handlers.
