# Action buttons

Declare a top-level `actions` array in your `schemas/mymod.json` settings schema:

```json
{
  "schemaVersion": 1,
  "title": "My mod",
  "groups": { "Maintenance": [] },
  "actions": [
    {
      "id": "rescan",
      "label": "Rescan animation files",
      "hint": "Reload the available animation list.",
      "group": "Maintenance",
      "confirmation": "Rescan animation files now?"
    }
  ]
}
```

`id` and `label` are required. IDs use 1–128 ASCII letters, digits, `_` or `-`,
and must be unique within the mod ignoring case. API calls use exact spelling.
`hint`, `group` and `confirmation` are optional. Omit confirmation to run on one
activation; when supplied it must be nonempty. The dialog initially selects
Cancel and requires a fresh acceptance. Long confirmation text scrolls with
the mouse wheel or Up/Down; Left/Right selects Cancel or Run Action.

Labels accept up to 256 UTF-8 bytes; hints and confirmation accept up to 4096.
Text is single-line UTF-8 without controls; the menu wraps it. An explicit group
must exist. Otherwise the first group is used, or an implicit General group for
an action-only schema with `"groups": {}`. Buttons follow ordinary settings and
precede hotkeys in their group, in authored action order.

Actions have no default or value, never enter values JSON, and are unaffected
by setting resets. The settings registry's value records remain settings-only.
Restart the game after changing declarations.

## C++

Include [OSFSettings.h](../sdk/OSFSettings.h) and initialize the settings client
at SFSE `kPostPostLoad`. The same client handles settings, hotkeys, and actions:

```cpp
OSFSettings::API::Client actions; // Process lifetime.

void OnRescan(std::uint64_t invocation, const char* mod, const char* id,
              void* context) noexcept
{
    auto& api = *static_cast<OSFSettings::API::Client*>(context);
    // Perform a short operation, or hand invocation to your existing work queue.
    // Call CompleteAction when the operation finishes, from any thread.
    api.CompleteAction(invocation, true, "Animation index reloaded.");
}

// In the kPostPostLoad listener:
if (actions.Init()) {
    auto result = actions.RegisterAction("mymod", "rescan", OnRescan, &actions);
    // Require OSFSettings::API::Status::Ok; log registration failures.
}
```

Exactly one native **or** Papyrus handler owns each declaration. Native
registration is process lifetime; another registration returns
`AlreadyRegistered`. Callback code and `context` must live until process exit.
Identity strings are borrowed until callback return.

Callbacks run on SFSE tasks with no main-thread guarantee. Return promptly and
schedule engine effects in the context your mod requires. Do not block waiting
for the player to close Settings. The menu pauses the game; your handler may
queue work that will run after it closes. OSF does not close menus, wait for
gameplay, create worker threads, or cancel the mod's work.

Complete immediately inside the callback or retain the token and complete
later. Returning is not completion. Messages are copied, allow at most 4096
single-line UTF-8 bytes, and may be omitted for a standard success/failure
message. Native callbacks are `noexcept`.

See the [buildable native example](../examples/actions/README.md).

## Papyrus

Register a bound quest/reference/alias from initialization and after each load:

```papyrus
Bool registered = OSFSettings.RegisterAction(Self, "mymod", "rescan")

Function OnOSFAction(String modId, String actionId, String invocation)
    ; Perform or queue the operation. Keep invocation as an opaque String.
    Bool completed = OSFSettings.CompleteAction(invocation, true, "Rescan finished.")
EndFunction
```

Global scripts use `RegisterActionStatic("MyScript", "mymod", "rescan")` and
the same callback marked `Global`. Repeating the same receiver registration
succeeds without adding another handler; a different owner is rejected.
VM submission rejection becomes an action failure. Accepted submission does
not guarantee eventual completion: implement exactly the three String arguments
shown above and always report completion.

The [Papyrus example](../examples/papyrus/README.md) includes an immediate reset
and a Global action that completes after `Utility.WaitMenuPause`.
Gameplay-dependent script work may remain pending until gameplay resumes.

## Lifetime and results

- Missing handlers show an unavailable button and explanation.
- Acceptance reserves the action before dispatch. Duplicate requests are
  rejected while it runs; other settings/actions remain usable.
- Closing Settings does not cancel or forget work. Reopening shows its current
  state or latest result. Results stay in memory, not in a save or values file.
- There is no timeout, retry queue or cancellation of running work. Missing
  completion leaves the button busy until session replacement or process exit.
- Only the first completion succeeds. Duplicate, stale and previous-session
  tokens cannot finish another invocation. Failure permits another click;
  it does not promise rollback of effects already performed.
- Load/Main Menu transitions suspend new requests. Session replacement
  invalidates outstanding tokens/results and removes Papyrus registrations;
  native handlers remain. Failed loads resume existing registrations. If dispatch
  encounters a transition in progress, it fails without invoking the handler.
- Invalidation cannot stop a native worker or a submitted VM call. Providers
  own work across loads and must prevent old work from changing a replacement
  world or overlapping newly requested operations.

Use a clear completion boundary: "Queue scene" can succeed when queued;
"Start scene" should succeed when it starts. Persistent problems can be reported
separately through diagnostics; failures do not automatically create issues.

## Validation

`xmake test osfsettings-action-tests/default` covers schema validation,
value-file exclusion, duplicate ownership/invocation, immediate and cross-thread
completion, stale tokens, session transitions, and native/Papyrus adapters.
The production build compiles normal/large movies and the Papyrus API.
Fresh in-game acceptance is still needed for confirmation/focus, controllers,
live Papyrus actions while paused, completion across close/reopen, and session
replacement. Existing release acceptance reports predate action buttons.
