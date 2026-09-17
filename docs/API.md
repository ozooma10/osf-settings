# API

C++ SDK: [OSFSettings.h](../sdk/OSFSettings.h). Requires CommonLibSF.
Papyrus is planned.

For native issue reporting, see [Mod Issues](DIAGNOSTICS.md).

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

## Native hotkey declarations

A schema can declare an optional top-level `hotkeys` array:

```json
"hotkeys": [
  { "id": "openMenu", "label": "Open mod settings", "default": "F10", "menu": "OSFSettingsMenu" }
]
```

`id` and `label` are required, nonempty strings. IDs use ASCII letters, digits,
underscores or hyphens and must be unique within the mod, ignoring letter case.
`default` is an optional, nonempty key-name string; omitting it means unbound.
Embedded NUL characters are rejected. At plugin load, key names are resolved
case-insensitively against the game's baked keyboard table. Unknown defaults
are logged and that declaration is skipped.

Declarations are loaded into `ModSchema::hotkeys`, separately from ordinary
setting values. A declaration becomes a native MainGameplay action named
`<mod id>/<hotkey id>`; the example above becomes `osfsettings/openMenu` for the
`osfsettings` mod. The plugin uses the native row formatter and adds the rows
at the start of vanilla's defaults before its parser runs, including on control
reset. The original caller continues loading saved overrides and resolving links.

Hotkeys appear alongside ordinary settings in the mod's groups. An optional
`group` field names an existing group ID (case-sensitive):

```json
{ "id": "openMenu", "label": "Open mod settings", "default": "F10", "menu": "OSFSettingsMenu", "group": "general" }
```

Omitting `group` appends the hotkey after the first declared group's settings.
Explicitly assigned hotkeys are appended after their target group's settings;
hotkeys within a group retain declaration order. If `groups` is empty, unassigned
hotkeys use an implicit **General** group. An explicit `group` must be a nonempty
string referring to a declared group; unknown IDs are schema errors. Group
placement does not change the native action name or binding persistence.

An optional `menu` field names a **registered native menu**, not a SWF filename:

```json
"hotkeys": [
  { "id": "openMenu", "label": "Open my menu", "default": "F4", "menu": "MyModMenu" }
]
```

The owning mod registers its menu factory with `RE::UI::RegisterMenu` and loads
its SWF in that menu's normal lifecycle. Register it before accepting gameplay
input. OSF does not create a menu class or register a factory from the SWF name.
The bundled Settings declaration targets `OSFSettingsMenu` through this same path.
`menu` must be a nonempty string without embedded NUL characters.

`HotkeyInput` installs its own `BSInputEventUserStandalone` in `MenuControls`.
It accepts enabled keyboard events for declarations with a menu target. All
edges reach native held-action tracking; a release (`value == 0`, nonnegative
`heldDownSecs`) sends `UIMessageQueue::AddMessage(menu, kShow)` and marks the
event stopped. Presses and repeats do not open a menu. The engine dispatcher
admits held actions and rejects unpaired releases before the button callback.
This is an open request; it does not toggle or close an already-open menu.

`OSFSettingsMenu` opens directly from gameplay and pauses the game through its
own menu flags. During native rebinding, it forwards input to the existing
SettingsDataModel receiver; no Pause parent or pending-open route is needed.
The bundled shortcut (F10 by default), the Pause MOD SETTINGS entry, and direct
show requests all open the same menu. Back from the mod list closes only OSF,
returning to gameplay or the already-open Pause menu. Hide/removal cancels any
active capture. Other mods' menu targets keep their own opening policy.

Menu declarations use control mask `0x08`, the same mask as keyboard Pause.
Declarations without `menu` retain the Movement mask and only register mappings;
OSF does not dispatch them to a public callback yet. Mappings are in MainGameplay;
there are no injected Pause-context links. The engine's active contexts,
control masks, disabled-event translation and held-action admission still apply.
Matching Pause's mask does **not** reproduce its private opening checks or its
availability in other contexts. Each menu owns its contexts and input behavior.

The input callback only enqueues a show message. Native `AddMessage` (ID 130659,
1.16.244.0) owns its queue lock and string references; the UI pump constructs and
opens the menu. No game-thread assumption or extra `BSService` task is needed
for that enqueue-only operation. The handler does not inspect player state or
menu stacks, impersonate Pause input, or intercept another menu's requests.

One startup hook attaches after native `InitializeHandlers`; the initialization
call (`99490 + 0x303 -> 114215`) is validated before patching. `MenuControls` and
the registered handler live until process exit. On 1.16.244, save reloads and
returning to the main menu preserve the same input service and registrations.
There is no cleanup hook or automatic handler destructor calling into the engine
during DLL teardown. No `MenuOpenHandler` vtable or Pause helper is modified.
See the local [vanilla input investigation](../../OSF%20RE/Investigations/Responses/2026-09-16-vanilla-menu-activation.md)
for the static dispatch and queue evidence. Live focus, loading, key capture and
transition behavior still need an in-game check; this path promises generic
show requests, not exact Pause eligibility.

The OSF menu embeds vanilla binding rows in its shared settings list and uses
native remapping and persistence. It displays the schema `label`; translating that label in the
game's own Controls panel and public hotkey callbacks remain separate work.
See [inline binding editor](NATIVE-BINDING-EDITOR.md) for the checkpoint's scope
and pending in-game checks. The required `groups` array may be
empty for a hotkey-only schema. Declarations are read once at startup; changes
require restarting the game.

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
