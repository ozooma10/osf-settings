# API

C++ SDK: [OSFSettings.h](../sdk/OSFSettings.h). Requires CommonLibSF.
Papyrus is planned.

## Usage

```cpp
OSFSettings::API::Client settings;

// During SFSE kPostPostDataLoad:
if (settings.Init() && settings.IsReady()) {
    std::string mode;
    auto status = settings.GetEnum("mymod", "mode", mode);
}
```

Consumers can initialize earlier, but must check `IsReady()` before reading.
Before the keyboard exists, named defaults use the guarded native embedded key table on Starfield 1.16.244.0.

- Reads: `GetBool`, `GetInt`, `GetFloat`, `GetEnum`, `GetKey`. Writes: matching `Set*` methods.
- Types: `bool`, `int64_t`, `double`, enum option string, keyboard virtual-key code (`uint32_t`).
- Check the returned `Status`. `Ok` means success; successful writes are already saved.
- `Reset(mod, key)` restores one default; `ResetMod(mod)` restores all defaults.
- Values live in `Data/SFSE/Plugins/OSF/Settings/values/<mod>.json`, across save games.
- The `Client` string overload owns its result in the calling mod and preserves it on error.

## Status

| Status | Meaning |
| --- | --- |
| `Ok` | Success; writes are saved. |
| `NotReady` | Client disconnected or service not ready. |
| `InvalidArgument` | Invalid mod ID/key or missing required argument. |
| `UnknownMod` | No loaded schema for this mod. |
| `UnknownSetting` | Key not found in the mod's schema. |
| `TypeMismatch` | Getter/setter type does not match the setting. |
| `InvalidValue` | Value fails the setting's bounds or allowed options. |
| `BufferTooSmall` | Enum buffer needs `required` bytes; the string overload handles this. |
| `SaveFailed` | Could not save; previous value is unchanged. |
| `UnknownSubscription` | Subscription token not found. |
| `InternalError` | Internal limit reached or unexpected internal status. |
| `UnknownHotkeyBlock` | Hotkey block token not found or already released. |
| `UnknownAction` | No hotkey declaration with this action ID exists for the mod. |

## Hotkeys (C++)

Hotkeys are named gameplay actions declared in the schema's top-level `hotkeys`
array. Ordinary `"type": "key"` settings remain stored settings and cannot be
subscribed as hotkeys. This replaces the previous raw-key observer contract.

```json
"hotkeys": [
  { "id": "toggleFeature", "label": "Toggle feature", "hint": "Enable or disable the feature.", "default": "F10" }
]
```

Definitions load once during plugin startup, before native ControlMap creation.
The stable engine name is `OSFSettings.<mod>.<action>`. IDs cannot contain dots;
use ASCII letters, digits, underscores or hyphens, at most 48 bytes. Full native
names fit 96 bytes. Case-only duplicate IDs are rejected. The installation
supports at most 64 declared actions across all schemas. Restart after changing
definitions. Labels may change without changing the persisted identity.

Actions use the existing gameplay context and activate once per keyboard press.
Optional `context` and `trigger` accept only `"gameplay"` and `"press"`.
`default` accepts a native keyboard name such as `"F10"`, `"L Ctrl"`, or `";"`,
or a numeric Win32 virtual-key code. Omission, `"UNBOUND"`, or 255 means unbound.
Escape and mouse-button VKs are rejected. Defaults are single keys; chord
defaults and mouse/gamepad callbacks are outside this version.

The default enters Starfield's defaults parser. A saved native user override
takes precedence, and vanilla Reset to Defaults recreates the schema default.
Changing a schema default requires a restart and does not intentionally overwrite
saved user choices. Choose an unused gameplay key for your default; defaults do
not run vanilla's interactive conflict checks.

```cpp
void OnToggle(const char* mod, const char* action, void* user) noexcept
{
    // Native game-thread command-drain phase. Toggle your feature or queue a
    // menu-open message. Do not enter a Scaleform movie from this callback.
}

OSFSettings::API::Subscription token{};
auto status = settings.SubscribeHotkey("mymod", "toggleFeature", OnToggle, nullptr, &token);
// Later, before destroying callback state:
settings.UnsubscribeHotkey(token);
```

- Subscribe after the provider initializes. It requires a declared action and
  available native backend, not a bound key. `UnknownAction` means no matching
  declaration. Registration does not change or reset a binding.
- Edit hotkeys in vanilla **Settings > Bindings**. Starfield owns primary and
  alternate slots, conflict handling, remapping, reset and `ControlMap_Custom`
  persistence. OSF has no second native-hotkey editor or binding cache.
  `GetKey`/`SetKey`, `Reset`, and `ResetMod` operate on ordinary settings, not
  these declarations. No hotkey value is saved in OSF's values JSON.
- Starfield selects one named action for an input event. OSF dispatches only
  that action's subscribers, never every action sharing a physical key. Multiple
  subscribers to the same action each receive its activation.
- Holds, repeats, releases, disabled events and events consumed by native UI do
  not activate callbacks. OSF uses vanilla's final resolved action name and
  `IsPressed()` result after original input delivery. It has no held-key table.
- Input admission uses native event state and explicit block tokens;
  gameplay eligibility is checked before each callback without a cached policy
  snapshot or recurring runtime poll. Pause,
  loading/reset, loss of focus, modal/input-blocking menus and
  Console suppress delivery. Native context flags also apply. Blocked presses
  are discarded; held keys must be released before a fresh activation.
- Callbacks run serially inside the verified `BSService::TaskQueue` drain on
  the game thread, after native input observation. No callback runs from the
  input worker or from the queue's inline fallback on an unverified thread.
  Callback code must not throw, block on a worker which is unsubscribing, or
  assume that this phase permits direct Scaleform movie calls.
- Unsubscribe invalidates queued calls and waits for an in-flight callback on
  another thread. Self-unsubscribe is allowed; keep callback state alive until
  that invocation returns. Tokens remain separate from setting-change tokens.
- Delivery carries resolved action identity, not the physical key. Native
  remap notifications, defaults rebuilds and menu open/close events cancel pending activations. Each
  input batch captures a cancellation generation before observation, so an old
  observation cannot be queued after remapping. OSF does not poll native maps.
- Pending activations are bounded at 256 deliveries. Overflow is dropped.
  Activations older than 250 ms, or invalidated by blocking, native remap notifications,
  reset or an unavailable backend, are discarded rather than replayed.
- A mod owns its feature/menu state. Press opens a menu; Escape/Back closes it.
  There is no same-hotkey-close bypass of gameplay suppression.

### Blocking for custom focused views

`AcquireHotkeyBlock(&token)` suppresses all OSF actions until every owner releases
its own token with `ReleaseHotkeyBlock(token)`. Acquire before a custom view
accepts keyboard input; release on close and failure paths. Acquisition cancels
pending activations, even if the token is released before delivery. A callback
already executing may finish. This does not block vanilla input or another
mod's independent input handling.

### Native backend and validation

The native defaults parser constructs the declared actions, including during
reset; OSF never appends directly to engine mapping arrays. The hook IDs and
instruction lengths were established against Starfield 1.16.244.0. There are
no startup version/RVA/instruction-byte checks or raw-key fallback.

The minimal implementation passes 685/685 host checks and builds both menu
variants. Fresh-game validation of the final event path and vanilla-only editing
is pending. Earlier production checks of F10 and saved overrides concern the
previous implementation. See [HOTKEYS.md](HOTKEYS.md) for the short architecture
walkthrough and focused acceptance sequence.

## Subscriptions

`Subscribe(mod, callback, user, &token)` watches one mod. `Unsubscribe(token)` removes it.

```cpp
void OnChanged(const char* mod, const char* key, void* user) noexcept;
```

- Subscribe before the first read.
- Initial callback: `key == nullptr`; reread all settings you use.
- Later callbacks identify changed keys. Changes may coalesce; null means reread all.
- API calls may come from any thread. Callbacks run serially on an SFSE task; no main-thread guarantee.
- Subscriptions and committed changes schedule notification tasks on demand.
  Writes during delivery queue a following task; there is no permanent polling task.
- Callback strings last only for that callback. Exceptions must not escape.
- Keep `user` alive until `Unsubscribe` returns. When unsubscribing inside a callback, keep it alive until that callback returns.
k that consumer effects run in their required context. Verify both menu sizes and controller input.
Compilation and file deployment do not replace this check.

The Papyrus bridge still needs native registration, conversion handling, receiver
validation, VM lifecycle handling, and a save/load consumer test.
k that consumer effects run in their required context. Verify both menu sizes and controller input.
Compilation and file deployment do not replace this check.

The Papyrus bridge still needs native registration, conversion handling, receiver
validation, VM lifecycle handling, and a save/load consumer test.
