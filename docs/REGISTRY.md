# Settings registry

Use the registry to discover mods and build a settings browser or cache.
Include [OSFSettingsRegistry.h](../sdk/OSFSettingsRegistry.h), which also includes
the [C++ client](API.md).

```cpp
#include "OSFSettingsRegistry.h"

void OnRegistry(const OSFSettings::API::RegistryView& view, void* context) noexcept;

// After settings.Init() at kPostPostLoad:
auto status = settings.ReadRegistry(nullptr, OnRegistry, nullptr); // All mods.
status = settings.ReadRegistry("mymod", OnRegistry, nullptr);      // One mod.
```

`Ok` invokes your callback exactly once, synchronously on the calling thread,
even for an empty registry. Errors do not invoke it. A missing mod returns
`UnknownMod`; an unavailable service returns `NotReady`.

Walk `mods/modCount` → `groups/groupCount` → `settings/settingCount`.
Each setting includes its key, label, hint, type, current/default values, restart
notice, and applicable bounds/options/editor limits. Native hotkey declarations
are separate and are not included as settings.

| `SettingView::type` | Active member of `value` / `defaultValue` |
| --- | --- |
| `Bool` | `boolean` |
| `Int` | `integer` |
| `Float` | `number` |
| `Enum`, `String` | `text` |
| `Key` | `key` |

Read only the active union member. Read bounds only when `hasMinimum` /
`hasMaximum` is true. The header describes the remaining type-specific fields.

All views, arrays, and strings expire when the callback returns. Copy retained
text with `std::string(text.data, text.size)` and copy any arrays you need.
Never modify or free provider memory. Exceptions must not escape your callback.
Each call gives a consistent snapshot; separate reads can observe newer values.

For a live cache: discover mods, subscribe to each, then read again before
publishing values. Refresh on notifications; a null key means refresh the whole
mod. Serialize refreshes, preserve the last good cache on failure, and retry.
Use IDs as identities, not display labels or array positions.

The [registry example](../examples/registry/README.md) implements copying,
subscription cleanup, and serialized refreshes.
