# API

C++ SDK: [OSFSettings.h](../sdk/OSFSettings.h). Requires CommonLibSF.
Papyrus is planned.

## Usage

```cpp
OSFSettings::API::Client settings;

// During SFSE kPostPostLoad:
if (settings.Init() && settings.IsReady()) {
    std::string mode;
    auto status = settings.GetEnum("mymod", "mode", mode);
}
```

- Reads: `GetBool`, `GetInt`, `GetFloat`, `GetEnum`. Writes: matching `Set*` methods.
- Types: `bool`, `int64_t`, `double`, enum option string.
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

## Hotkey declarations (schema only)

A schema can declare an optional top-level `hotkeys` array:

```json
"hotkeys": [
  { "id": "openMenu", "label": "Open mod settings", "default": "F10" }
]
```

`id` and `label` are required, nonempty strings. IDs use ASCII letters, digits,
underscores or hyphens and must be unique within the mod, ignoring letter case.
`default` is an optional, nonempty key-name string; omitting it means unbound.
Embedded NUL characters are rejected. Key names remain text at this checkpoint;
native name resolution and validation belong to the later binding integration.

Declarations are loaded into `ModSchema::hotkeys`, separately from ordinary
setting values. This checkpoint adds no native bindings, input handlers or
callbacks. The existing required `groups` array can be empty for a hotkey-only schema.

## Subscriptions

`Subscribe(mod, callback, user, &token)` watches one mod. `Unsubscribe(token)` removes it.

```cpp
void OnChanged(const char* mod, const char* key, void* user) noexcept;
```

- Subscribe before the first read.
- Initial callback: `key == nullptr`; reread all settings you use.
- Later callbacks identify changed keys. Changes may coalesce; null means reread all.
- API calls may come from any thread. Callbacks run serially on an SFSE task; no main-thread guarantee.
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
