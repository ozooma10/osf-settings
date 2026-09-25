# Settings

Define settings and [action buttons](#action-buttons) in one JSON schema, then connect them to your mod through [C++](#c-integration) or [Papyrus](#papyrus-integration).

OSF Settings provides a centralized interface for users to view and edit those settings.

## Define your settings

Ship `Data/SFSE/Plugins/OSF/Settings/schemas/mymod.json` with your mod:

```json
{
  "title": "My mod",
  "groups": {
    "General": [
      {
        "key": "enabled",
        "type": "bool",
        "label": "Enable feature",
        "default": true
      }
    ]
  }
}
```

The filename without `.json` is your mod ID: `mymod.json` gives `mymod`.
OSF builds your mod's configuration menu from this file. Schemas load at startup; restart Starfield after editing them.

Player values are saved separately for each mod in `Data/SFSE/Plugins/OSF/Settings/values/<modId>.json` and shared across save games.

Ship just the schema file; OSF Settings manages your mod's values file.

A schema that fails to load is shown as an error in the menu's **Mod Issues** tab, and the details are written to `OSF Settings.log` under `Documents/My Games/Starfield/SFSE/Logs`.

Display text can be translated with separate [localization catalogs](LOCALIZATION.md) without modifying the schema. IDs and stored values remain unchanged.

### Schema fields and types

- The required root field is `groups`. `title` defaults to the mod ID; `description` is optional. `schemaVersion` is optional and must be `1` when present.
- The mod ID comes from the schema filename without `.json`.
- Mod IDs use lowercase ASCII letters, digits, `.`, `_`, or `-`; empty IDs, `.` and `..` are invalid.
- Group names become headings. Groups and their controls appear in authored order.
- Every setting needs `key`, `type`, and `default`. Keys must be nonempty and unique across the mod. API lookups use exact, case-sensitive keys.
- Optional `label` defaults to the key. `hint` adds help text.

| Type | Default example | Additional fields |
| --- | --- | --- |
| `bool` | `true` | None |
| `int` | `3` | Optional inclusive `min` / `max` |
| `float` | `0.75` | Optional inclusive `min` / `max`; positive `step` defaults to `0.1` |
| `enum` | `"normal"` | Required `options`: value-to-label object or string array |
| `string` | `"auto"` | `maxLength`: 1–4096 UTF-8 bytes; defaults to 256 |
| `key` | `"F4"` | `allowUnbound` defaults to true; C++ reads/writes keyboard VK codes |

Defaults must satisfy the setting's type and limits.

Float `step` controls the editor increment; writes need not be multiples of it.

Enum option values are unique, nonempty strings; the default and API values must match a value exactly. Options appear in authored order. Use an object to give each value a display label:

```json
{
  "key": "mode",
  "type": "enum",
  "default": "normal",
  "options": {
    "quiet": "Quiet",
    "normal": "Normal",
    "verbose": "Verbose"
  }
}
```

When the labels are also the values, use a string array:

```json
"options": ["quiet", "normal", "verbose"]
```

Object labels must be strings; an empty label falls back to its value. Labels may repeat. Only values are saved, so changing labels or order preserves existing selections.

Strings are single-line UTF-8. Empty text is allowed; NUL, control characters, and line/paragraph separators are rejected. Limits count bytes, not characters. Values are preserved without trimming or truncation.

A `key` setting stores a value for your own input handler. Use [hotkeys](KEYBINDINGS.md) for OSF-dispatched actions. Key defaults accept recognized names or bindable VK integers. Unbinding and `"UNBOUND"` / `255` defaults are allowed unless `allowUnbound` is explicitly `false`.

Add `"requires": "restart"` to show a restart notice. This is only a notice: values still save immediately, and your mod decides when to apply them.

## C++ integration

Add [OSFSettings.h](../sdk/OSFSettings.h) to your plugin's includes. Initialize the client at SFSE `kPostLoad` or later and check `IsReady()`. OSF Settings loads schemas and values and starts its services during its own plugin load callback. `kPostLoad` is the earliest point independent of plugin load order; `kPostPostLoad` is also valid but is not required. Game language has separate readiness: `GetLanguage()` can return `NotReady` until translation resources load.

```cpp
#include "OSFSettings.h"

OSFSettings::API::Client settings;

// In your kPostLoad handler:
if (settings.Init() && settings.IsReady()) {
    bool enabled = true;
    if (settings.GetBool("mymod", "enabled", &enabled) == OSFSettings::API::Status::Ok) {
        // Apply enabled to your mod.
    }
}
```

`Init()` returns false if a compatible OSF Settings API is unavailable.
Initialize before sharing the client across threads. This example reads once; subscribe as described below if your mod needs live changes.

| Read / write | C++ value |
| --- | --- |
| `GetBool` / `SetBool` | `bool` |
| `GetInt` / `SetInt` | `std::int64_t` |
| `GetFloat` / `SetFloat` | `double` |
| `GetEnum` / `SetEnum` | Option ID string |
| `GetString` / `SetString` | Free-form UTF-8 string |
| `GetKey` / `SetKey` | `std::uint32_t` keyboard VK code; `kUnboundKey` is 255 |

Scalar getters take an output pointer. Enum/string getters also accept an owning `std::string&`; setters accept strings directly. Getter/setter types must match exactly.

```cpp
auto status = settings.SetBool("mymod", "enabled", false);
status = settings.Reset("mymod", "enabled");
status = settings.ResetMod("mymod");
```

Check each returned `Status`. `Ok` means success, including an unchanged value; writes and resets are already saved. Failed writes preserve the previous value.
Common errors are `NotReady`, `UnknownMod`, `UnknownSetting`, `TypeMismatch`, `InvalidValue`, and `SaveFailed`. All statuses and raw buffer signatures are in [the header](../sdk/OSFSettings.h).

### Watch for changes

Subscribe before your first read if the mod needs live updates:

```cpp
void OnChanged(const char* mod, const char* key, void* context) noexcept;
OSFSettings::API::Subscription token{};

auto status = settings.Subscribe("mymod", OnChanged, nullptr, &token);
// On success, read your initial values. Keep token for Unsubscribe(token).
```

- Implement `OnChanged` to reread current values. `key == nullptr` means refresh the whole mod (usually a settings reset).
- Callbacks run serially on an SFSE task, without a main-thread guarantee. Schedule engine/UI work in its required context.

The registry example's [RegistryConsumer.h](../examples/registry/RegistryConsumer.h) demonstrates complete subscription, refresh, and cleanup ownership.

## Papyrus integration

Compile against [OSFSettings.psc](../data/Scripts/Source/OSFSettings.psc), supplied
in `Scripts/Source`. Use a Global script for the settings handler:

```papyrus
ScriptName MyModSettings Hidden

Function InitializeSettings() Global
    If OSFSettings.IsReady()
        If OSFSettings.RegisterForChangesStatic("MyModSettings", "mymod")
            ReadSettings()
        EndIf
    EndIf
EndFunction

Function ReadSettings() Global
    Bool enabled = OSFSettings.GetBool("mymod", "enabled", true)
    ; Apply enabled to your mod here.
EndFunction

Function OnOSFSettingChanged(String modId, String key) Global
    ReadSettings()
EndFunction
```

Call `MyModSettings.InitializeSettings()` from your mod's existing initialization and post-load path. An owning quest or alias must call it after each load. Registrations are cleared on load. Repeating a registration succeeds without adding duplicates.

Subscribe before reading. Implement `OnOSFSettingChanged` as a Global function with exactly two String parameters. Bound quest/reference scripts can use `RegisterForChanges` instead.

### Read, write, and reset

```papyrus
Bool enabled = OSFSettings.GetBool("mymod", "enabled", true)
Bool saved = OSFSettings.SetBool("mymod", "enabled", !enabled)
```

- Reads: `GetBool`, `GetInt`, `GetFloat`, `GetEnum`, `GetString`. Each takes `(modId, key, fallback)` and returns the fallback on failure.
- Writes: matching `Set*` functions take `(modId, key, value)`. `Reset(modId, key)` and `ResetMod(modId)` restore defaults.
- Writes/resets return `true` when saved; failure leaves the old value unchanged. Check the result; failures are logged in `OSF Settings.log`.
- IDs, keys, and enum options use the schema's exact spelling. Enums and strings are distinct types. There are no Papyrus `GetKey` / `SetKey` functions.
- Papyrus integers are 32-bit and floats are single precision. Reads that overflow return the fallback; float precision may be reduced.

See the [instance](../examples/papyrus/OSFSettingsExample.psc) and [Global](../examples/papyrus/OSFSettingsExampleGlobal.psc) examples, with their [schema](../examples/papyrus/papyrusexample.json), for all value types, hotkeys, and action callbacks.

## Action buttons

Declare a `type: "action"` control inside a group in your `schemas/mymod.json` settings schema:

```json
{
  "title": "My mod",
  "groups": {
    "Maintenance": [
      {
        "type": "action",
        "id": "rescan",
        "label": "Rescan animation files",
        "hint": "Reload the available animation list.",
        "confirmation": "Rescan animation files now?"
      }
    ]
  }
}
```

`type: "action"`, `id` and `label` are required. API calls use exact spelling. `hint` and `confirmation` are optional.

### C++ action handlers

```cpp
OSFSettings::API::Client actions; // Process lifetime.

void OnRescan(std::uint64_t invocation, const char* mod, const char* id, void* context) noexcept
{
    auto& api = *static_cast<OSFSettings::API::Client*>(context);
    // Perform a short operation, or hand invocation to your existing work queue.
    // Call CompleteAction when the operation finishes, from any thread.
    api.CompleteAction(invocation, true, "Animation index reloaded.");
}

// In the kPostLoad listener:
if (actions.Init()) {
    auto result = actions.RegisterAction("mymod", "rescan", OnRescan, &actions);
}
```

Exactly one native **or** Papyrus handler owns each declaration. Native registration is process lifetime; another registration returns `AlreadyRegistered`.

Callbacks run on SFSE tasks with no main-thread guarantee. The menu pauses the game; your handler may queue work that will run after it closes.
OSF does not close menus, wait for gameplay, create worker threads, or cancel the mod's work.

Complete immediately inside the callback or retain the token and complete later.

See the [native example](../examples/actions/main.cpp) and its [schema](../examples/actions/osfsettings-actions-example.json).

### Papyrus action handlers

Register a bound quest/reference/alias from initialization and after each load:

```papyrus
Bool registered = OSFSettings.RegisterAction(Self, "mymod", "rescan")

Function OnOSFAction(String modId, String actionId, String invocation)
    ; Perform or queue the operation. Keep invocation as an opaque String.
    Bool completed = OSFSettings.CompleteAction(invocation, true, "Rescan finished.")
EndFunction
```

Global scripts use `RegisterActionStatic("MyScript", "mymod", "rescan")` and the same callback marked `Global`.
Repeating the same receiver registration succeeds without adding another handler; a different owner is rejected.

The [instance example](../examples/papyrus/OSFSettingsExample.psc) includes an immediate reset, and the [Global example](../examples/papyrus/OSFSettingsExampleGlobal.psc) has an action that completes after `Utility.WaitMenuPause`.
Gameplay-dependent script work may remain pending until gameplay resumes.

## Related features

- [Hotkeys](KEYBINDINGS.md): declare rebindable actions and handle them in C++ or Papyrus, or open a registered native menu.
- [Menu launchers](LAUNCHERS.md): add a top-level `menus` object keyed by ID for native menus on the Home tab, or use the separate C++ launcher service for custom interfaces. Each schema entry has a `title` and a registered `menu` name; `description` is optional. Menu-only mods can use `"groups": {}`. Menus do not create stored values.
- [Issue reporting](DIAGNOSTICS.md): report and clear problems in Mod Issues from C++ or Papyrus.
