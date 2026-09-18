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

## Restart-required settings

Any ordinary setting can declare `"requires": "restart"`:

```json
{
  "key": "developerMode",
  "type": "bool",
  "label": "Developer mode",
  "default": false,
  "requires": "restart"
}
```

When selected, its details begin with **Changes take effect after restarting
Starfield.**, followed by its hint. The notice also appears when the value is
unchanged or at its default. Omit `requires` for settings without this notice;
when present, only the exact string `"restart"` is accepted. Other values are
schema errors. This field applies to ordinary settings, not native hotkey declarations.

This is presentation metadata. Writes still save immediately, reads return the
saved value, and change notifications run normally. The owning mod decides when
to apply its settings. Slim does not defer changes or track whether a restart
is pending. The development design preview includes a restart-required setting
under **Advanced**.

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
| `UnknownHotkeyBlock` | Block token is zero, unknown, or already released. |
| `UnknownHotkey` | Hotkey ID not found in the mod's declarations. |
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
It accepts enabled keyboard events for registered declarations. For menu targets,
all edges reach native held-action tracking; a release (`value == 0`, nonnegative
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
Declarations without `menu` retain the Movement mask (`0x401`) and support
registered callbacks on key-down. Mappings are in MainGameplay;
there are no injected Pause-context links. The engine's active contexts,
control masks, disabled-event translation and held-action admission still apply.
Matching Pause's mask does **not** reproduce its private opening checks or its
availability in other contexts. Each menu owns its contexts and input behavior.

For menu targets, the input handler only enqueues a show message. Native `AddMessage` (ID 130659,
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
game's own Controls panel remains separate work.
See [inline binding editor](NATIVE-BINDING-EDITOR.md) for the checkpoint's scope
and pending in-game checks. The required `groups` array may be
empty for a hotkey-only schema. Declarations are read once at startup; changes
require restarting the game.

## Native hotkey callbacks

Omit `menu` to declare a callback hotkey. No callback function name or additional
type field is needed in JSON:

```json
{
  "schemaVersion": 1,
  "id": "mymod",
  "title": "My mod",
  "groups": [],
  "hotkeys": [
    { "id": "toggleFeature", "label": "Toggle feature", "default": "F6" }
  ]
}
```

Place this schema at `Data/SFSE/Plugins/OSF/Settings/schemas/mymod.json`.
The hotkey appears in the implicit General group and uses the same native binding
editor and persistence as menu hotkeys. An explicit `group` must name a declared
group, as described above. Omitting `default` creates an initially unbound hotkey
that can still accept callback registrations.

After `Client::Init()` at SFSE `kPostPostLoad`, register using the exact mod and
hotkey IDs. The callback is a C++ function pointer with an optional owner pointer:

```cpp
void OnHotkey(const char* mod, const char* id, void* user) noexcept;

auto status = settings.RegisterHotkey("mymod", "toggleFeature", OnHotkey, owner);
// OnHotkey and owner remain valid until process exit.
```

See the complete [native hotkey example](../examples/hotkeys/README.md) for a
buildable SFSE consumer and its schema.

- `RegisterHotkey` requires initialized settings, native mappings and task submission; otherwise
  it returns `NotReady`. Null required arguments or malformed IDs return
  `InvalidArgument`. The `user` pointer may be null if the callback does not need it.
- Unknown mods return `UnknownMod`; missing or differently cased hotkey IDs return
  `UnknownHotkey`. Menu-target declarations return `TypeMismatch`. A callback
  declaration skipped because its default key is unknown returns `InvalidValue`.
- Registrations last until process exit. There is no token or unregister call;
  keep the callback code and its `user` object alive for that lifetime. Register
  once per owner, rather than on every save load. Each successful call adds a
  callback, including repeated registration of the same function and owner.
- Each accepted key-down submits one `SFSE::TaskInterface::AddTask` task capturing
  the callbacks present at that moment. That task invokes them in registration
  order. Separate presses are not coalesced. Registration sends no initial
  notification, and later registrations receive no earlier presses.
- A fresh down (`value > 0`, `heldDownSecs == 0`) submits delivery. Repeats and
  releases do not activate callbacks. Input is stopped only when a press is
  submitted for at least one callback; declarations without callbacks remain unconsumed.
- SFSE owns task scheduling. Callbacks run outside the input handler and internal
  locks; OSF adds no main-thread or cross-task serialization guarantee. Schedule engine effects in
  their required context. Callback strings last through the call, and exceptions
  must not escape. Accepted tasks always invoke their captured callbacks, even if
  a hotkey block is acquired before execution or by an earlier callback in the task.
- Callback hotkeys use gameplay control eligibility: the native Movement mask,
  active contexts, disabled-event filtering, keyboard-only input and native
  binding capture still apply. Registering a callback does not change availability
  or the saved binding. Use the focus block API below for custom input owners.

## Blocking hotkeys for focused views

Use `AcquireHotkeyBlock(&token)` before granting a web view or other input owner
focus, and `ReleaseHotkeyBlock(token)` after ending capture. These calls are
thread-safe and need no settings schema or settings readiness. Initialize the
SDK client at `kPostPostLoad` as usual. Acquisition returns `Ok` and a nonzero
token; a failed call preserves the output. Keep one token per capture lifetime
and release it on close, cancellation, failed opening, or teardown.

```cpp
OSFSettings::API::HotkeyBlock block{};
// Before granting capture; refuse capture if acquisition fails.
auto status = settings.AcquireHotkeyBlock(&block);
// After revoking capture, including error/cleanup paths:
if (status == OSFSettings::API::Status::Ok) {
    settings.ReleaseHotkeyBlock(block);
    block = 0;
}
```

Each acquisition is independent, including repeated acquisitions by one caller.
Releasing one token leaves all other blocks active. Blocks affect menu and callback
hotkeys dispatched by OSF Settings; they do not disable Starfield controls,
consume keyboard events, close menus, or stop another mod's input handler.
Declarations handled directly by another mod need that mod's own focus policy.
The native binding editor holds its own token while capturing a binding.

Acquisition discards held menu presses and prevents new callback presses from
being accepted. It does not cancel tasks for previously accepted presses, and
tasks do not recheck blocks when they run. External blocks leave button edges
available to native held-action bookkeeping, but OSF does not arm or dispatch
menu opens or accept new callback presses. Native context and binding-editor filters still apply.
After the last token is released, callback activation requires a fresh down;
menu activation also requires the matching release for the same physical key
and action. Repeats and releases from keys held before or during capture cannot
replay input rejected during the block. Previously accepted callbacks still run,
and an admitted menu release may finish; a block cannot retract a show message
already submitted to the engine. The existing native admission rules still apply.

### OSF UI integration checkpoint

Source inspection of OSF UI's current focus menu found no blocking menu flags
or pushed input contexts; its receiver stops gamepad events only, and its
control layer leaves the `0x08` mask used by Slim's menu actions enabled.
Its window-message capture therefore does not establish native hotkey exclusion.
The block API supplies explicit coordination without altering shared control masks.

OSF UI still uses its older Settings ABI. When porting `OSFSettingsClient`, replace
its suppression calls with these methods and retain the token until capture is
revoked. Handle acquisition failure before allowing capture and balance cleanup
on view closure, failed reveal, browser-host failure, and focus teardown. Passive
HUDs do not need a block. Use `RegisterHotkey` with a process-lifetime owner for
hotkey callbacks; the rest of the OSF UI adapter port remains separate work.

Host checks cover nested/concurrent owners, typing during a block, held releases,
capture cancellation, native handler enqueueing, and shared API/input state.
They do not establish native focus or engine dispatch behavior in a running game.
After the UI adapter port, use `SettingsSmoke` in the OSF Test Harness to check
capturing versus passive views, close before key release, failed opening/host
recovery, and focus loss/restoration. Fresh hotkey presses must work after cleanup.

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
