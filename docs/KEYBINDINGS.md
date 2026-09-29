# Hotkeys

Add a top-level `hotkeys` object keyed by hotkey ID to your [schema](SETTINGS.md), `schemas/mymod.json`:

```json
{
  "title": "My mod",
  "groups": {},
  "hotkeys": {
    "toggle": { "label": "Toggle feature", "default": "F6" }
  }
}
```

Players rebind these actions in OSF Settings or vanilla Controls. Starfield saves bindings in `ControlMap_Custom.txt`; they are separate from ordinary settings values. Schema changes require restarting the game.

Hotkeys support keyboard and controller bindings independently. Controller bindings start unbound and invoke the same C++ or Papyrus callback, or open the same declared menu. OSF Settings follows Starfield's active input device: use the controller to display and edit controller bindings, or the keyboard/mouse to display PC bindings. The Keybindings page shows controller buttons and the native button glyphs in controller mode. Capture stays within the device family selected at its start; Menu/Start cancels controller capture, and Escape cancels either mode. Occupied bindings require confirmation before the native remapper can swap them. Controller bindings use the native primary slot.

This applies to registered `hotkeys`. Ordinary `type: "key"` settings retain their existing keyboard VK contract (and optional mouse buttons); they do not store controller IDs. Analog stick movement and custom hold/combo gestures are not added by this integration; controller buttons and trigger button events use the game's native remapper.

- Each object key is a nonempty hotkey ID; `label` is required. IDs use ASCII letters, digits, `_`, or `-` and must be unique within the mod, ignoring case. API calls use exact casing. Do not include an `id` field inside the declaration.
- Hotkeys appear in authored order within their group. Omit `hotkeys` or use `{}` when there are none.
- Omit `default` to start unbound. Defaults are keyboard key names, matched without case sensitivity.
- Optional `group` names an existing schema group. Otherwise the hotkey follows the first group's settings, or appears in General when `groups` is empty.

To migrate an existing array, move each declaration's `id` to its object key and remove the inner `id` field. The array form is no longer accepted.

## C++ callbacks

For a C++ callback, register once after [client initialization](SETTINGS.md#c-integration) at SFSE `kPostLoad`:

```cpp
void OnHotkey(const char* mod, const char* id, void* context) noexcept;
auto status = settings.RegisterHotkey("mymod", "toggle", OnHotkey, nullptr);
```

See the [C++ example](../examples/hotkeys/main.cpp) and its [schema](../examples/hotkeys/osfsettings-hotkeys-example.json).

## Papyrus callbacks

Register a bound quest/reference/alias from initialization and after each load, following the [Papyrus settings lifecycle](SETTINGS.md#papyrus-integration):

```papyrus
Bool registered = OSFSettings.RegisterHotkey(Self, "mymod", "toggle")

Function OnOSFHotkey(String modId, String hotkeyId)
    ; Run your action once per accepted key-down.
EndFunction
```

Global scripts use `RegisterHotkeyStatic("MyScript", "mymod", "toggle")` with the same callback marked `Global`.

See the [instance](../examples/papyrus/OSFSettingsExample.psc) and [Global](../examples/papyrus/OSFSettingsExampleGlobal.psc) examples.

## Open a native menu

To open a native menu instead, add `"menu": "MyModMenu"` to the declaration. Your plugin must register that name with `RE::UI::RegisterMenu`. Menu hotkeys cannot also register callbacks.

## Block hotkeys while capturing input

For a custom view that captures input, acquire a block before granting focus:

```cpp
OSFSettings::API::HotkeyBlock block{};
auto status = settings.AcquireHotkeyBlock(&block);
// Grant focus only on Ok. After revoking focus, including cancellation/cleanup:
// settings.ReleaseHotkeyBlock(block);
```

Call `ReleaseHotkeyBlock(block)` for every acquired token. All blocks must be released before new presses can activate.
