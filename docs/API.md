# C++ settings

For menu buttons with confirmation/completion, use the same client's
`RegisterAction` and `CompleteAction` methods; see [action buttons](ACTIONS.md).

Add [OSFSettings.h](../sdk/OSFSettings.h) to your plugin's includes. It requires
CommonLibSF. [Declare your settings](SETTINGS.md), then initialize the client
at SFSE `kPostPostLoad`:

```cpp
#include "OSFSettings.h"

OSFSettings::API::Client settings;

// In your kPostPostLoad handler:
if (settings.Init() && settings.IsReady()) {
    bool enabled{};
    if (settings.GetBool("mymod", "enabled", &enabled) == OSFSettings::API::Status::Ok) {
        // Apply enabled to your mod.
    }
}
```

`Init()` returns false if a compatible OSF Settings API is unavailable.
Initialize before sharing the client across threads.

| Read / write | C++ value |
| --- | --- |
| `GetBool` / `SetBool` | `bool` |
| `GetInt` / `SetInt` | `std::int64_t` |
| `GetFloat` / `SetFloat` | `double` |
| `GetEnum` / `SetEnum` | Option ID string |
| `GetString` / `SetString` | Free-form UTF-8 string |
| `GetKey` / `SetKey` | `std::uint32_t` keyboard VK code; `kUnboundKey` is 255 |

Scalar getters take an output pointer. Enum/string getters also accept an owning
`std::string&`; setters accept strings directly. Getter/setter types must match
exactly. Failed reads preserve the output.

```cpp
std::string mode;
auto status = settings.GetEnum("mymod", "mode", mode);
status = settings.SetEnum("mymod", "mode", "quiet");
status = settings.Reset("mymod", "mode");
status = settings.ResetMod("mymod");
```

Check each returned `Status`. `Ok` means success, including an unchanged value;
writes and resets are already saved. Failed writes preserve the previous value.
Common errors are `NotReady`, `UnknownMod`, `UnknownSetting`, `TypeMismatch`,
`InvalidValue`, and `SaveFailed`. All statuses and raw buffer signatures are in
[the header](../sdk/OSFSettings.h).

Subscribe before your first read if the mod needs live updates:

```cpp
void OnChanged(const char* mod, const char* key, void* context) noexcept;
OSFSettings::API::Subscription token{};
auto status = settings.Subscribe("mymod", OnChanged, nullptr, &token);
// On success, read your initial values. Keep token for Unsubscribe(token).
```

- Implement `OnChanged` to reread current values. `key == nullptr` means refresh
  the whole mod, including the initial notification. Changes may coalesce.
- Callbacks run serially on an SFSE task, without a main-thread guarantee.
  Schedule engine/UI work in its required context; exceptions must not escape.
- Callback strings last only for the call. Keep your context alive until a
  successful `Unsubscribe(token)` returns. If unsubscribing inside the callback,
  keep it alive until that callback returns.

Other systems: [hotkeys](Keybindings.md), [registry](REGISTRY.md),
[issue reporting](DIAGNOSTICS.md).
