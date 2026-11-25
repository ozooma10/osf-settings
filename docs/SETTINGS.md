# Settings

Define settings and [action buttons](#action-buttons) in one JSON schema, then connect them to your mod through [C++](#c-integration) or [Papyrus](#papyrus-integration).

OSF Settings provides a centralized interface for users to view and edit those settings.

## Define your settings

Ship `Data/SFSE/Plugins/OSF/Settings/schemas/mymod.json` with your mod:

```json
{
  "$schema": "https://raw.githubusercontent.com/ozooma10/osf-settings/dev/sdk/osf-settings.schema.json",
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

Player values are saved separately for each mod in `<Documents>/My Games/Starfield/OSF/Settings/<modId>.json`. Documents is resolved through Windows, including redirected locations such as OneDrive. Values are shared across save games, mod-manager profiles, and game installations using that Documents folder. Launcher history is stored in the same folder in `internal.json`, so the mod id `internal` is reserved.

Ship just the schema file; OSF Settings manages your mod's values file.

A schema that fails to load is shown as an error in the menu's **Mod Issues** tab, and the details are written to `OSF Settings.log` under `Documents/My Games/Starfield/SFSE/Logs`.

Display text can be translated with separate [localization catalogs](LOCALIZATION.md) without modifying the schema. IDs and stored values remain unchanged.

### Schema fields and types

The [JSON Schema](../sdk/osf-settings.schema.json) supplies editor completion and
validation. The URL above follows the development branch; pin a released tag for
reproducible authoring. Runtime validation additionally checks identifiers,
cross-field relationships and UTF-8 byte limits.

- The required root field is `groups`. `title` defaults to the mod ID; `description` is optional. `schemaVersion` is optional and must be `1` when present. `extends` puts the controls on [another mod's page](#extend-another-mods-page).
- The mod ID comes from the schema filename without `.json`.
- Mod IDs use 1–128 lowercase ASCII letters, digits, `.`, `_`, or `-`; `.` and `..` are invalid. Windows device names such as `con`, `nul`, `com1`, and `lpt1` are rejected, including names followed by an extension such as `nul.extra`.
- Group names become headings. Groups and their controls appear in authored order.
- Every setting needs `key`, `type`, and `default`. Keys must be nonempty and unique across the mod, ignoring ASCII case. API lookups use exact, case-sensitive keys.
- Optional `label` defaults to the key. `hint` adds help text.

| Type | Default example | Additional fields |
| --- | --- | --- |
| `bool` | `true` | None |
| `int` | `3` | Optional inclusive `min` / `max` |
| `float` | `0.75` | Optional inclusive `min` / `max`; positive `step` defaults to `0.1` |
| `enum` | `"normal"` | Required `options`: value-to-label object or string array |
| `string` | `"auto"` | `maxLength`: 1–4096 UTF-8 bytes; defaults to 256 |
| `key` | `"F4"` | `allowUnbound` defaults to true; C++ reads/writes keyboard VK codes |
| `color` | `"#336699"` | `alpha` defaults to false. Added in settings ABI 1.1 |

Defaults must satisfy the setting's type and limits.

Float `step` controls the editor increment; writes need not be multiples of it.

Enum option values are strings, unique ignoring ASCII case; the default and API values must match a value exactly. An empty string is allowed when explicitly declared as an option. Options appear in authored order. Use an object to give each value a display label:

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

A `key` setting stores a value for your own input handler. Use [hotkeys](KEYBINDINGS.md) for OSF-dispatched actions. Key defaults accept recognized names or bindable VK integers. Unbinding and `"UNBOUND"` / `255` defaults are allowed unless `allowUnbound` is explicitly `false`. `allowMouse: true` also permits the five physical mouse buttons (`MOUSE1` through `MOUSE5`, VK `1, 2, 4, 5, 6`). [Runtime providers](PROVIDERS.md) can observe key settings without consuming input.

A `color` default is `"#RRGGBB"` or `"#RRGGBBAA"`, with hex digits in either case. Without `"alpha": true` the setting is opaque: only `#RRGGBB` or an alpha of `FF` is accepted, and the picker hides transparency. Saved values use uppercase `#RRGGBB` for opaque colors and `#RRGGBBAA` otherwise.

```json
{ "key": "markerTint", "type": "color", "label": "Marker tint", "default": "#FF8000" },
{ "key": "glow", "type": "color", "label": "Glow", "default": "#FF800080", "alpha": true }
```

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
| `GetColor` / `SetColor` | `std::uint32_t` packed `0xRRGGBBAA`, or `Rgba` channels (`UnpackColor` / `PackColor`) |

Scalar getters take an output pointer. Enum/string getters also accept an owning `std::string&`; setters accept strings directly. Getter/setter types must match exactly.

Color calls are part of settings ABI 1.1. Through `Client` they return `Unsupported` when the installed OSF Settings only provides 1.0; check `settings.Has(OSFSettings::API::kColorVersion)` to fall back early.

```cpp
OSFSettings::API::Rgba tint;
if (settings.GetColor("mymod", "markerTint", &tint) == OSFSettings::API::Status::Ok) {
    // tint.r, tint.g, tint.b, tint.a are 0-255; a is 255 unless the setting allows alpha.
}
```

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

- Reads: `GetBool`, `GetInt`, `GetFloat`, `GetEnum`, `GetString`, `GetColor`. Each takes `(modId, key, fallback)` and returns the fallback on failure.
- Writes: matching `Set*` functions take `(modId, key, value)`. `Reset(modId, key)` and `ResetMod(modId)` restore defaults.
- Writes/resets return `true` when saved; failure leaves the old value unchanged. Check the result; failures are logged in `OSF Settings.log`.
- IDs, keys, and enum options use the schema's exact spelling. Enums and strings are distinct types. There are no Papyrus `GetKey` / `SetKey` functions.
- Colors are text in Papyrus: `GetColor` always returns `"#RRGGBBAA"`, and `SetColor` accepts `"#RRGGBB"` or `"#RRGGBBAA"`.
- Papyrus integers are 32-bit and floats are single precision. Reads that overflow return the fallback; float precision may be reduced.

See the [instance](../examples/papyrus/OSFSettingsExample.psc) and [Global](../examples/papyrus/OSFSettingsExampleGlobal.psc) examples, with their [schema](../examples/papyrus/papyrusexample.json), for all value types, hotkeys, and action callbacks.

## Versions and dependencies

OSF Settings 1.2.0 can report missing or outdated supporting mods. Declare your
own version and minimum dependency versions in the schema:

```json
{
  "title": "My mod",
  "version": "1.0.0",
  "nexusModId": 12345,
  "dependencies": {
    "osfsettings": "1.2.0",
    "basemod": "2.0.0"
  },
  "groups": {}
}
```

- `version` and dependency versions are numeric `major.minor.patch` strings.
  Prerelease labels and version ranges are not supported.
- Dependency keys are settings mod IDs (schema filenames), not DLL or plugin
  filenames. Only loaded schemas/runtime providers participate; this is not a
  scan of every installed mod. A metadata-only schema can use `"groups": {}`.
- A missing dependency produces a warning. An installed dependency with a
  declared version is compared against the requested minimum. An installed
  dependency without a version cannot be compared and produces no version warning.
  OSF Settings uses its running DLL version for its own comparison.
- Warnings do not disable the consumer's settings or enforce a load order.
  Reports refresh when runtime providers change. Valid dependency metadata can
  still appear in Mod Issues when the rest of a schema fails validation.
- `nexusModId` is your mod's numeric Starfield Nexus ID (replace the example).
  Omit it or use `0` if unavailable. Issue details can open the provider's Nexus
  page when that provider's schema supplies an ID; a missing provider has no
  metadata from which to obtain its link.

See the [dependency examples](../examples/dependencies/osf-stack.json).

### Migrating older version declarations

The former `expectedSettingsVersion` field is ignored in 1.2.0. Replace
`"expectedSettingsVersion": "1.1.2"` with
`"dependencies": { "osfsettings": "1.2.0" }` when adopting the new features.
For a schema that still supports older OSF Settings releases, keep the old field
alongside `dependencies` so those older releases retain their version warning.
The new `version` field describes your own mod, not the required OSF Settings
version. The former special-case OSF UI DLL version check has also been removed;
declare an `osfui` dependency when your mod needs it, and have the provider publish
its version through a schema or runtime registration.

## Extend another mod's page

A mod that builds on another mod can put its settings on that mod's page instead of a page of its own. Add a root `extends` naming the host's mod ID:

```json
{
  "title": "Base Mod - Combat Extras",
  "extends": "basemod",
  "groups": {
    "Combat": [
      { "key": "recoil", "type": "float", "default": 0.5, "label": "Extra recoil" }
    ]
  }
}
```

Save this as `Data/SFSE/Plugins/OSF/Settings/schemas/basemod_combatextras.json`.
This feature requires OSF Settings 1.2.0. Only the page changes: your mod ID is
still your filename, your values still live in `<modId>.json`, and API calls use
your own ID: `GetFloat("basemod_combatextras", "recoil")`. Actions and hotkeys also
retain their original owner. Contributed settings identify their source in the
details pane.

`extends` makes the host a dependency: if it is not installed, your settings appear on their own page and Mod Issues reports the host as missing.
Add an explicit `dependencies` entry when you need a minimum version instead. A mod cannot extend itself, and a loop of mods extending each other leaves each on its own page with an issue reported.

### Optional integrations

For settings that only matter when the host is installed, make the host optional:

```json
"extends": { "mod": "basemod", "optional": true }
```

With the host installed this behaves exactly like `"extends": "basemod"`.
Without it, no missing-host warning is reported and your controls, hotkeys and
launchers are hidden. Your schema still loads, so `Get*`/`Set*` calls keep working
and saved values are kept. Other declared dependencies are still checked. An
explicit minimum for the optional host is checked when that host is installed.

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

Action callbacks run inline when the menu invokes them, outside the service lock. Return promptly and schedule engine/UI work in its required context. The menu pauses the game; your handler may queue work that will run after it closes.
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
