# Settings

Ship a JSON schema and connect its values to your mod through [C++](#c-integration) or [Papyrus](#papyrus-integration).

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
OSF builds your mods configuration menu from this file. Schemas load at startup; restart Starfield after editing them. 

Player values are saved in `Data/SFSE/Plugins/OSF/Settings/state.json` and shared across save games.

Ship just the schema file; OSF Settings manages the shared state file. Existing per-mod values are imported automatically; see [Persistence](PERSISTENCE.md). 

Load failures are reported in `OSFSettings.log` as well as the "Mod Health" Section of OSF Settings menu.

Display text can be translated with separate [localization catalogs](LOCALIZATION.md) without modifying the schema. IDs and stored values remain unchanged.

### Schema fields and types

- The required root field is `groups`. `title` defaults to the mod ID;
  `description` is optional.
- The mod ID comes from the schema filename without `.json`. A legacy root `id`
  is optional; if present, it must be a nonempty string matching that ID exactly.
- `schemaVersion` is optional and defaults to `1` when omitted. If present, it
  must be the integer `1`; other versions and value types are rejected.
- Mod IDs use lowercase ASCII letters, digits, `.`, `_`, or `-`; empty IDs,
  `.` and `..` are invalid. Use a unique filename and keep it stable between releases.
  Renaming the file changes the mod ID used by saved settings, translation catalogs,
  and API calls. Change `title` to rename the displayed mod without changing its ID.
- Group names become headings. Groups and settings appear in authored order.
- Every setting needs `key`, `type`, and `default`. Keys must be nonempty and
  unique across the mod. API lookups use exact, case-sensitive keys.
- Optional `label` defaults to the key. `hint` adds help text.

| Type | Default example | Additional fields |
| --- | --- | --- |
| `bool` | `true` | None |
| `int` | `3` | Optional inclusive `min` / `max`; signed 64-bit |
| `float` | `0.75` | Optional inclusive `min` / `max`; positive `step` defaults to `0.1` |
| `enum` | `"normal"` | Required `options` array; optional matching `optionLabels` array |
| `string` | `"auto"` | `maxLength`: 1–4096 UTF-8 bytes; defaults to 256 |
| `key` | `"F4"` | `allowUnbound` defaults to false; C++ reads/writes keyboard VK codes |

Defaults must satisfy the setting's type and limits. Float `step` controls the
editor increment; writes need not be multiples of it. Enum options are unique,
nonempty strings; the default and API values must match an option exactly.

```json
{
  "key": "mode",
  "type": "enum",
  "default": "normal",
  "options": ["quiet", "normal", "verbose"],
  "optionLabels": ["Quiet", "Normal", "Verbose"]
}
```

Strings are single-line UTF-8. Empty text is allowed; NUL, control characters,
and line/paragraph separators are rejected. Limits count bytes, not characters.
Values are preserved without trimming or truncation.

A `key` setting stores a value for your own input handler. Use [hotkeys](Keybindings.md)
for OSF-dispatched actions. Key defaults accept recognized names or bindable VK
integers; `"UNBOUND"` / `255` requires `allowUnbound: true`.

Add `"requires": "restart"` to show a restart notice. This is only a notice:
values still save immediately, and your mod decides when to apply them.

## C++ integration

Add [OSFSettings.h](../sdk/OSFSettings.h) to your plugin's includes. It requires
CommonLibSF. Initialize the client at SFSE `kPostPostLoad`:

```cpp
#include "OSFSettings.h"

OSFSettings::API::Client settings;

// In your kPostPostLoad handler:
if (settings.Init() && settings.IsReady()) {
    bool enabled = true;
    if (settings.GetBool("mymod", "enabled", &enabled) == OSFSettings::API::Status::Ok) {
        // Apply enabled to your mod.
    }
}
```

`Init()` returns false if a compatible OSF Settings API is unavailable.
Initialize before sharing the client across threads. This example reads once;
subscribe as described below if your mod needs live changes.

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
auto status = settings.SetBool("mymod", "enabled", false);
status = settings.Reset("mymod", "enabled");
status = settings.ResetMod("mymod");
```

Check each returned `Status`. `Ok` means success, including an unchanged value;
writes and resets are already saved. Failed writes preserve the previous value.
Common errors are `NotReady`, `UnknownMod`, `UnknownSetting`, `TypeMismatch`,
`InvalidValue`, and `SaveFailed`. All statuses and raw buffer signatures are in
[the header](../sdk/OSFSettings.h).

### Watch for changes

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
- Callback strings last only for the call. Keep the client and context alive
  until a successful `Unsubscribe(token)` returns. If unsubscribing inside the
  callback, keep the context alive until that callback returns.

The [registry example](../examples/registry/README.md) demonstrates complete
subscription, refresh, and cleanup ownership.

## Papyrus integration

Compile against [OSFSettings.psc](../data/Scripts/Source/OSFSettings.psc), supplied
in `Scripts/Source`. OSF Settings supplies the compiled provider PEX; no framework
ESM is needed. Attach this script to a player reference alias in your own running
quest:

```papyrus
ScriptName MyModSettings extends ReferenceAlias

Event OnInit()
    InitializeSettings()
EndEvent

Event OnPlayerLoadGame()
    InitializeSettings()
EndEvent

Function InitializeSettings()
    If OSFSettings.IsReady()
        If OSFSettings.RegisterForChanges(Self, "mymod")
            ReadSettings()
        EndIf
    EndIf
EndFunction

Function ReadSettings()
    Bool enabled = OSFSettings.GetBool("mymod", "enabled", true)
    ; Apply enabled to your mod here.
EndFunction

Function OnOSFSettingChanged(String modId, String key)
    ReadSettings()
EndFunction
```

Subscribe before reading. Implement `OnOSFSettingChanged` as a function with
exactly two String parameters. An empty callback key requests a full refresh,
including the initial notification. Notifications may coalesce; reread current
values rather than counting changes.

Registrations are cleared on load or return to the main menu. Register from
`OnInit` and again from a player alias's `OnPlayerLoadGame`, as above. Repeating a
registration succeeds without adding duplicates. Keep the receiving script alive.
Other bound quest/reference scripts can also receive callbacks.

Global scripts use `RegisterForChangesStatic("MyScript", "mymod")` with the same
callback marked `Global`. An owning quest/alias must register them after each
load. Papyrus schedules callbacks; delivery is not synchronous.

### Read, write, and reset

```papyrus
Bool enabled = OSFSettings.GetBool("mymod", "enabled", true)
Bool saved = OSFSettings.SetBool("mymod", "enabled", !enabled)
```

- Reads: `GetBool`, `GetInt`, `GetFloat`, `GetEnum`, `GetString`. Each takes
  `(modId, key, fallback)` and returns the fallback on failure.
- Writes: matching `Set*` functions take `(modId, key, value)`.
  `Reset(modId, key)` and `ResetMod(modId)` restore defaults.
- Writes/resets return `true` when saved; failure leaves the old value unchanged.
  Check the result; failures are logged in `OSFSettings.log`.
- IDs, keys, and enum options use the schema's exact spelling. Enums and strings
  are distinct types. There are no Papyrus `GetKey` / `SetKey` functions.
- Papyrus integers are 32-bit and floats are single precision. Reads that overflow
  return the fallback; float precision may be reduced.

See the [instance and Global example](../examples/papyrus/README.md) for all value
types, hotkeys, and action callbacks. Include your compiled consumer PEX and
quest in your mod; depend on OSF Settings rather than bundling another provider
DLL/PEX.

## Related features

- [Hotkeys](Keybindings.md): declare rebindable actions and handle them in C++ or
  Papyrus, or open a registered native menu.
- [Action buttons](ACTIONS.md): add a top-level `actions` array for buttons with
  optional confirmation and asynchronous completion. Use `RegisterAction` and
  `CompleteAction` in C++ or Papyrus. Actions have no stored value/default.
- [Menu launchers](LAUNCHERS.md): add a top-level `menus` array for native menus
  in the Launcher tab, or use the separate C++ launcher service for custom
  interfaces. Each schema entry has `id`, `title`, and a registered `menu` name;
  `description` is optional. Menu-only mods can use `"groups": {}`. Menus do not
  create stored values and use the same schema version.
- [Settings registry](REGISTRY.md): discover mods, metadata, and current values
  from C++ to build a browser or cache.
- [Issue reporting](DIAGNOSTICS.md): report persistent problems in Mod Issues
  from C++.
