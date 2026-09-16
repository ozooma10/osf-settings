# Native gameplay hotkeys

Declare an action, let Starfield bind it, and subscribe to its name.

```text
Schema action + default
        -> vanilla ControlMap and Settings > Bindings
        -> native named press event
        -> OSF subscriber on the game thread
```

The bundled `osfsettings.openMenu` action defaults to F10. Its subscriber opens
OSF Settings; Escape/Back closes the menu. Players change the hotkey in vanilla
**Settings > Bindings**, where it appears as **OSF Settings: Open mod settings**.
Starfield owns primary/alternate bindings, conflicts, resets and persistence.
There is no second hotkey editor or binding-value file in OSF.

## Code to read

- `Plugin.cpp` calls `NativeHotkeys::Install()` during load and `Start()` after
  game data loads. It subscribes the open-menu callback like any other consumer.
- `NativeHotkeys.cpp` connects schema declarations and labels to vanilla,
  receives native input, and schedules callback delivery.
- `HotkeyService.cpp` holds the immutable action catalog and SDK subscriptions.
  `ProcessInput()` calls vanilla first, then uses the resulting action name and
  `ButtonEvent::IsPressed()` to queue a callback.
- `Runtime.cpp` handles ordinary settings. It has no hotkey scheduling or input state.

`NativeHotkeys` and `HotkeyService` are the only hotkey implementation pairs.
The service retains subscriber lifetime handling, explicit block tokens and
pending-call cancellation because these are promises made by the SDK.

## Defaults and bindings

Schema declarations use stable IDs and an optional `"default": "F10"` (or a
supported native keyboard name / numeric Win32 VK). Omitting the default gives
UNBOUND. The engine identity is `OSFSettings.<mod>.<action>`; changing a label
keeps the saved identity. Schema changes require a restart.

CommonLibSF's `ControlMap::ParseMappings` overload inserts declarations into
MainGameplay defaults, after Jump, and invokes the native parser. Starfield
loads saved overrides afterward. Defaults rebuilding follows the same path.
Subscribing never binds a key or replaces the player's choice.

The parser and translator hooks are still needed: the exposed native interfaces
provide no incremental register-action-and-label operation. The input hook
observes vanilla's existing receiver; the remap hook cancels pending activations.
CommonLibSF owns the typed native operations and detour helper. See
[`input-controls.md`](../lib/commonlibsf/docs/input-controls.md) for their
contracts, addresses and executable evidence.

## Delivery

OSF calls the original UI receiver once, then reads unconsumed, enabled keyboard
presses by their native action names. Vanilla's `IsPressed()` and held time own
press/repeat interpretation. OSF has no physical-key matching, held-key table,
pre/post event snapshot, or binding cache. Borrowed input pointers stay inside
that synchronous call and never enter a task.

A nonempty activation batch schedules a one-shot SFSE task, which posts to the
native `BSService::TaskQueue`. This keeps the native queue's inline fallback out
of the input callback. Delivery checks the native drain owner before invoking
subscribers, preserving the SDK's game-thread contract. There is no permanent
task or runtime poll. No work is scheduled while idle.

Gameplay eligibility is checked before each callback: pause, loading/reset,
loss of focus, Console and modal/input-blocking menus suppress delivery. Menu
transitions, remaps, defaults rebuilding and explicit block tokens cancel
pending activations. A generation captured before vanilla input processing
also cancels a batch invalidated during that call. Pending deliveries are
bounded at 256 and expire after 250 ms; dropped input is not retried.

Unsubscribe cancels queued calls and waits for an executing callback on another
thread. Self-unsubscribe is supported. Custom focused views use
`AcquireHotkeyBlock` / `ReleaseHotkeyBlock`; native definitions remain intact.

Ordinary `type: "key"` settings and their capture UI are separate settings values.
They remain editable in OSF and do not dispatch these native hotkey actions.

## Validation

The minimal implementation passes 685/685 host checks, covering native final
name/state selection, press/repeat filtering, task coalescing, cancellation,
subscription lifetime, settings persistence and the CommonLibSF interfaces.
The plugin and normal/large-text menu SWFs build successfully.

The hook IDs, offsets and instruction lengths were established against Starfield
1.16.244.0. There are no startup game-version, RVA or instruction-byte checks.
A successful build does not establish support for another executable layout.
Earlier production runs verified F10 opening the menu and native overrides
surviving restart; those runs used the previous implementation. Fresh-game
validation of this smaller implementation remains pending.

Check in a fresh game session:

1. F10 opens OSF Settings once; holding does not reopen it after closing.
2. Change the action to F11 in vanilla Bindings. F11 opens OSF Settings and F10
   stops doing so. Restart and check that the override persists.
3. Exercise vanilla primary/alternate bindings, conflict handling, unbinding
   and Reset to Defaults.
4. Try Pause, Console, text entry, loading and alt-tab, including held keys
   across transitions. Blocked presses must not replay.
5. Open OSF Settings through Pause and check ordinary settings and key capture.
   There should be no native-hotkey editor or binding-request wait screen.
