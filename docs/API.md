# API

C++ SDK: [OSFSettings.h](../sdk/OSFSettings.h). Requires CommonLibSF.
Papyrus: [script API, callbacks, and lifecycle](PAPYRUS.md).

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

- Reads: `GetBool`, `GetInt`, `GetFloat`, `GetEnum`, `GetString`. Writes: matching `Set*` methods.
- Types: `bool`, `int64_t`, `double`, enum option string, free-form string.
- Check the returned `Status`. `Ok` means success; successful writes are already saved.
- `Reset(mod, key)` restores one default; `ResetMod(mod)` restores all defaults.
- Values live in `Data/SFSE/Plugins/OSF/Settings/values/<mod>.json`, across save games.
- The `Client` string overload owns its result in the calling mod and preserves it on error.

## Registry discovery and snapshots

`ISettings::ReadRegistry(mod, callback, context)` and the matching `Client` method
expose the loaded settings registry without knowledge of internal store types.
Include `OSFSettingsRegistry.h` for the registry descriptors; it also includes
`OSFSettings.h`. Ordinary settings consumers only need `OSFSettings.h`.
Pass `nullptr` for all mods or an exact, case-sensitive mod ID for one mod:

```cpp
#include "OSFSettingsRegistry.h"

void OnRegistry(const OSFSettings::API::RegistryView& registry, void* context) noexcept;

auto status = settings.ReadRegistry(nullptr, OnRegistry, owner); // Discover all mods.
status = settings.ReadRegistry("mymod", OnRegistry, owner);      // Refresh one mod.
```

On `Ok`, the callback runs exactly once, including when there are no loaded mods.
It receives `RegistryView::mods/modCount`; each `ModView` contains
`groups/groupCount`, and each `GroupView` contains `settings/settingCount`.
Empty arrays have a null pointer and zero count. All successfully loaded mods
are included, even if they have no ordinary settings. Failed schemas are not
registry entries. Mod order is provider order; group, setting, and enum-option
order matches the parsed schema. Use IDs as identities, never array indices.

The descriptors are independent, standard-layout, trivially copyable public
records, with no STL containers, internal type aliases, or allocator ownership.
They contain this metadata:

| Descriptor | Fields and meaning |
| --- | --- |
| `ModView` | Authored ID, title, description, and groups |
| `GroupView` | Group name as both ID and label, and settings, including empty groups |
| `SettingView` | Key, label, hint, exact `SettingType`, restart requirement, current value, and default value |
| Integer metadata | Optional inclusive `int64_t` minimum/maximum; editor increment is implicitly 1 |
| Float metadata | Optional inclusive `double` minimum/maximum and editor `step`; step is not a storage-validity grid |
| Enum metadata | Ordered option identifiers and corresponding display labels |
| String metadata | Effective `maxLength` in UTF-8 bytes, excluding NUL |
| Key metadata | `allowUnbound`; current/default values are native keyboard VK codes, including `kUnboundKey` |

`SettingView::type` selects the active member of both `value` and `defaultValue`:
`Bool → boolean`, `Int → integer`, `Float → number`, `Enum/String → text`,
`Key → key`. Bounds use `integer` or `number` and are readable only when their
respective `hasMinimum`/`hasMaximum` flag is true. Options exist only for Enum;
`step` is meaningful only for Float, `maxLength` only for String, and
`allowUnbound` only for Key. Other metadata fields remain zero/false/null; do
not read inactive union members. Enum current/default text is the option ID,
not its label. Integers remain exact 64-bit integers, without conversion through
double. Public enum values are explicit and unrelated to internal variant indices.

All text uses `TextView { data, size }`. `data` is non-null, NUL-terminated UTF-8;
`size` is the byte length excluding the final NUL. Copy with
`std::string(text.data, text.size)`. Presentation strings and group IDs may
contain embedded NULs; using only `strlen` would lose data. Mod IDs, setting keys,
enum IDs, and free-form values follow their existing validation contracts.
Text is the parsed schema text with existing fallback labels applied; this API
does not localize or invent additional metadata.

### Ownership, consistency, and threading

The provider copies the authoritative store under the service mutex, then builds
the complete public projection before invoking consumer code. Metadata, defaults,
and current values in one snapshot describe a single instant, across all selected
mods. This is the same store used by typed getters, writes, resets, and the native
menu. There is no second registry, schema-file rescan, or persistent public cache.

The callback runs synchronously on the calling thread, with no provider locks
held. Calls may originate on any thread; simultaneous reads may overlap. Reads,
writes, subscriptions, and nested reads are allowed in the callback. Concurrent
or reentrant writes cannot alter the snapshot currently being read. A nested
read or an ordinary getter may observe newer values. Separate calls do not form
one transaction. Schemas and the mod set are fixed after startup; changes to
schema files require restarting the game.

Every descriptor, array, and string is read-only and valid only until that
callback returns. Copy anything to be retained. Copying a descriptor alone does
not extend the lifetime of its pointers. The provider owns and releases all
snapshot memory; consumers must neither modify nor free it. Caller arguments
must remain valid during the call. The callback and `context` are not retained.

The callback must be `noexcept`: an exception escaping it terminates the process.
If copying can fail, handle that inside the callback and record a consumer-side
result. Provider `Ok` means snapshot delivery succeeded, not that the consumer's
processing succeeded. Do not perform engine/UI effects unless the calling
context permits them; neither registry reads nor change notifications create
a main-thread guarantee.

### Errors

Errors never invoke the callback. The provider validates the callback and mod ID
before checking readiness, then performs lookup and projection:

| Status | Meaning |
| --- | --- |
| `InvalidArgument` | Null callback, or a non-null mod ID that fails the existing mod-ID rules; empty is not a wildcard |
| `NotReady` | Provider has not started; a detached Client also returns this |
| `UnknownMod` | A well-formed filtered mod ID is not loaded |
| `InternalError` | Snapshot allocation/projection failed, or a length/count cannot fit the public `uint32_t` field |

`context` may be null. There is no buffer probe, truncation, partial delivery,
callback cancellation, or snapshot-release call. Registry reads cause no writes
or notifications. This extends the pre-launch 1.0 interface; rebuild consumers
against the current SDK and provider together, since older pre-launch 1.0 DLLs
do not have this method. Existing slots remain in place.

### Notifications and OSF UI bridge requirements

Discover mod IDs, subscribe to each mod, then read values again before publishing
an initial cache. Do not publish values captured before subscription. A write
between discovery and subscription is covered by that later read; writes after
subscription also produce invalidations. A pending invalidation may describe a
value already included in a snapshot, so duplicate current-value refreshes are
normal. Serialize capture and replacement in the consumer if more than one
thread can refresh its cache.

`ChangedFn` supplies an invalidation, not a value. On a keyed notification,
reread the key using its known type or read the affected mod. On
`key == nullptr` (including the initial notification and a changed `ResetMod`),
read the mod and refresh all its values. Never manufacture a setting identity
for null. Existing subscription lifetime, coalescing, and unsubscribe guarantees
remain unchanged. If a refresh fails, retain the last good cache and retry the
invalidation rather than publishing partial data as current.

Inspection of OSF UI's `SettingsModule`, `SettingsMirror`, and
`SettingsSubscriptions` establishes these integration requirements:

- Retained `osfui/settings` state contains mods, schema groups/settings, and
  current values. The public descriptors supply Slim's supported subset,
  including defaults and editor metadata. Initial publication or a full refresh
  can rebuild that retained state from a complete snapshot.
- `SettingsMirror` owns values indexed by authored mod/key identities. Copy the
  snapshot before replacing its contents; do not retain provider views or use
  display labels as identities.
- Native subscription replay requires one current value per setting, serialized
  as a value rather than display text. Refresh the mirror before generating those
  replay entries. A null-key invalidation therefore expands into current-value
  entries; it cannot be forwarded directly as a `settings.changed` event. Existing
  OSF UI replay ordering and duplicate tolerance remain adapter responsibilities.
- Localization, native VK-to-OSF-UI-key-name conversion, case-insensitive lookup
  policy, and any web numeric-representation policy belong to the adapter. This
  API preserves Slim's identities, numeric precision, and key representation.

Native `hotkeys` declarations are separate engine-managed actions and are not
ordinary settings values. This registry does not project them as key settings.
Historical OSF UI features that Slim does not support (such as flags, pages,
conditions, presets, and injected conflict/keyboard data) are not synthesized.
This change does not implement or port the OSF UI adapter.

The buildable [registry example](../examples/registry/README.md) demonstrates
discovery, copying, refresh serialization, null-key handling, and unsubscribe
cleanup using only the public SDK.

## Free-form strings

Use `"type": "string"` for editable single-line text. `default` is required and
must be a string; an empty string is valid. For example, OSF UI's language setting:

```json
{
  "key": "language",
  "label": "Language",
  "type": "string",
  "default": "auto",
  "maxLength": 32,
  "hint": "Use auto for Starfield's language, or enter a locale such as en, de, or pt-BR."
}
```

`maxLength` counts **UTF-8 bytes**, excluding the terminating NUL used by C++
buffers. Omit it for a limit of **256 bytes**, or provide an integer from **1 to
4096**, inclusive. Zero, negative, fractional, null, boolean, and larger limits
are schema errors. The default must fit the limit. For example, `é` needs two
bytes and `😀` needs four. No path truncates an overlong value to make it fit.

Defaults, saved overrides, C++ writes, and menu edits use the same text policy:
valid UTF-8 Unicode scalar values; no NUL, C0 controls (`U+0000–001F`), DEL/C1
controls (`U+007F–009F`), or line/paragraph separators (`U+2028`, `U+2029`). This
rejects tabs and line breaks. Empty text, spaces, case, punctuation, and other
Unicode text are preserved exactly, without trimming or normalization. Invalid
defaults reject the schema. Invalid saved overrides report a load error and keep
that setting's default; a malformed JSON/UTF-8 file keeps defaults for the file.
Invalid writes return `InvalidValue` and leave the stored value unchanged.

Strings and enums have distinct types. `GetString`/`SetString` on an enum and
`GetEnum`/`SetEnum` on a string return `TypeMismatch`. Enum values still require
an exact authored option, and their existing validation is unchanged. String
settings do not need an `options` list and accept arbitrary text within their
validation contract.

```cpp
std::string language;
auto status = settings.GetString("osfui", "language", language);
status = settings.SetString("osfui", "language", "pt-BR");
status = settings.SetString("osfui", "language", std::string_view("auto"));
```

`ISettings::GetString(mod, key, out, capacity, &required)` uses a caller-owned
UTF-8 buffer. `required` includes the final NUL; even empty text needs one byte.
Pass `nullptr, 0` to query the size (`BufferTooSmall`). A short buffer returns
`BufferTooSmall`, updates `required`, and leaves the buffer untouched. Other
errors preserve both outputs. Null `required`, or null `out` with nonzero
capacity, is `InvalidArgument`. Retry if a concurrent write grows the value
between query and copy. `Client::GetString(..., std::string&)` handles this retry,
owns the allocation in the caller, and changes the output only on `Ok`.

`ISettings::SetString(mod, key, value, length)` copies exactly `length` UTF-8
bytes. The length excludes a terminator, and input need not be terminated.
`value` must be non-null, including for an empty value (`"", 0`). Lengths above
4096 return `InvalidValue` before reading the buffer. Embedded NULs within the
explicit length are rejected. The Client accepts this same signature, a
`std::string_view` (also accepting `std::string`), or a C string. The C-string
overload reads through the first NUL; use a view or explicit length for data
whose length is known. Caller inputs are borrowed only during the call.

Successful changes are saved as ordinary JSON strings in the existing values
file before the service publishes them or queues change notifications. A failed
save returns `SaveFailed` with the previous value, file, and notifications
unchanged. Equal values are successful no-ops. `Reset` saves the authored string
default and invalidates that key; `ResetMod` saves all defaults atomically and
requests a full refresh. Unchanged resets do not notify. String changes use the
same subscriptions, coalescing, and readiness rules as other setting types.
These methods extend the repository's pre-launch 1.0 API contract.

The Scaleform row opens an editable text field. Enter or the **SAVE** button
commits the draft; Escape, **CANCEL**, focus loss, and menu removal discard it.
The editor displays the byte limit, keeps invalid or unsaved drafts open, and
uses the native text-input context so typing does not trigger menu shortcuts.
Reset restores the default outside an active edit. Entry uses a physical
keyboard; no on-screen keyboard is supplied. Both normal and large-text SWFs
contain the control. Fresh-game input, focus, glyph coverage, and controller
behavior still require the SettingsSmoke harness when in-game testing is requested.

### OSF UI language consumer

Source inspection of `OSF UI/data/OSFUI/settings/osfui.json` found the declaration
above. `Runtime::OnSettingChanged` detects the game language only for the exact
string `"auto"`; other strings go through `LocalizationService::NormalizeLocale`
(trimming, underscore-to-dash conversion, known aliases, and locale casing).
Localization tries the requested locale, its base language, then English.
Slim preserves the authored value and leaves that interpretation to the consumer;
there is no fixed locale enumeration.

[The language fixture](../tests/fixtures/osfui-language.json) copies only this
declaration into a Slim `schemaVersion: 1` envelope. It is test data, not an
installed OSF UI schema. OSF UI still needs a separate adapter/schema port;
this change does not connect or modify its runtime.

Run `xmake build osfsettings-string-tests` then `xmake run osfsettings-string-tests`
for focused schema, UTF-8, byte-limit, persistence/reload, save-failure, reset,
notification, type-separation, and caller-buffer checks. The development design
preview includes the language field under **Advanced**.

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
| `InvalidValue` | Value fails the setting's bounds, text validation, or allowed options. |
| `BufferTooSmall` | Enum or string buffer needs `required` bytes; the owning overload handles this. |
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
`group` field names an existing key in the `groups` object (case-sensitive):

```json
{ "id": "openMenu", "label": "Open mod settings", "default": "F10", "menu": "OSFSettingsMenu", "group": "General" }
```

Omitting `group` appends the hotkey after the first declared group's settings.
Explicitly assigned hotkeys are appended after their target group's settings;
hotkeys within a group retain declaration order. If `groups` is empty, unassigned
hotkeys use an implicit **General** group. An explicit `group` must be a nonempty
string referring to a declared group name; unknown names are schema errors. Group
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

`HotkeyInput` installs its own `OSFSettings::BSInputEventUserStandalone` in `MenuControls`.
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
native remapping and persistence. It displays the schema `label`. Vanilla Bindings
places registered OSF hotkeys in one **Mod Bindings** section before the vanilla
sections, using `<mod title>: <hotkey label>` for each row. Mods are sorted by title
(ignoring ASCII letter case), then ID for ties; hotkeys retain declaration order.
Schema groups organize the OSF menu only. This is presentation: native action IDs,
MainGameplay contexts, conflicts, and saved bindings retain their existing meaning.
See [inline binding editor](NATIVE-BINDING-EDITOR.md) for the checkpoint's scope
and pending in-game checks. The required `groups` object may be
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
  "groups": {},
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
void OnHotkey(const char* mod, const char* id, void* context) noexcept;

auto status = settings.RegisterHotkey("mymod", "toggleFeature", OnHotkey, owner);
// OnHotkey and owner remain valid until process exit.
```

See the complete [native hotkey example](../examples/hotkeys/README.md) for a
buildable SFSE consumer and its schema.

- `RegisterHotkey` requires initialized settings, native mappings and task submission; otherwise
  it returns `NotReady`. Null required arguments or malformed IDs return
  `InvalidArgument`. The `context` pointer may be null if the callback does not need it.
- Unknown mods return `UnknownMod`; missing or differently cased hotkey IDs return
  `UnknownHotkey`. Menu-target declarations return `TypeMismatch`. A callback
  declaration skipped because its default key is unknown returns `InvalidValue`.
- Registrations last until process exit. There is no token or unregister call;
  keep the callback code and its `context` object alive for that lifetime. Register
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

`Subscribe(mod, callback, context, &token)` watches one mod. `Unsubscribe(token)` removes it.

```cpp
void OnChanged(const char* mod, const char* key, void* context) noexcept;
```

- Subscribe before the first read.
- Initial callback: `key == nullptr`; reread all settings you use.
- Later callbacks identify changed keys. Changes may coalesce; null means reread all.
- API calls may come from any thread. Callbacks run serially on an SFSE task; no main-thread guarantee.
- Callback strings last only for that callback. Exceptions must not escape.
- Keep `context` alive until `Unsubscribe` returns. When unsubscribing inside a callback, keep it alive until that callback returns.

Papyrus uses the same change service through session-scoped registrations.
See [Papyrus acceptance](PAPYRUS.md#build-and-acceptance) for its host checks and
the outstanding fresh-game VM/lifecycle acceptance.
