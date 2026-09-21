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

For a C++ callback, omit `menu` and register once after [client initialization](API.md)
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

[Papyrus](PAPYRUS.md) can register the same declarations through `RegisterHotkey`.

To open a native menu instead, add `"menu": "MyModMenu"` to the declaration.
Your plugin must register that name with `RE::UI::RegisterMenu` before accepting
input. OSF requests the menu on key release; it does not toggle it closed.
Menu hotkeys cannot also register callbacks. Native gameplay input rules apply;
OSF dispatch currently handles keyboard input.

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

See the [buildable C++ example](../examples/hotkeys/README.md).
