# Inline native bindings

Hotkey declarations are shown in a mod's Hotkeys section using `ControlsList`
and `Shared.Components.SystemPanels.SettingsControlListEntry` from the imported
`SettingsPanel.swf` / `SettingsPanel_LRG.swf`. The authored `Binding` cells retain
their selected/listening states. Schema labels replace the stock translation-key
text. `RemapConfirmation` supplies vanilla's occupied-binding confirmation.

The movie subscribes to `ControlBindingsData`, filters MainGameplay rows by the
declared action name, and retains native main/alternate binding data. It opens
the native settings session and binding category (4), then uses the component's
`SettingsPanel_RemapMode` and `SettingsPanel_ClearBinding` requests. Completion
validates through `SettingsPanel_ValidateControls` and saves through
`SettingsPanel_SaveControls`; the saved notification updates the status text.
There is no second OSF value for these native bindings.

This checkpoint supports editing the native PC rows, including the bundled
`osfsettings/openMenu`. Select either populated slot and press **X** / **Clear
Binding** to clear it. OSF sends the existing `SettingsPanel_ClearBinding` event
with that slot's priority; the native model rebuilds and saves the result.
Per-action reset is not exposed: the native category reset resets **all game
controls**. Controller dispatch is separate work.

During OSF-owned keyboard/mouse capture, taking a key assigned to a different
action in MainGameplay always requests the vanilla conflict popup, including
when the edited slot is populated or the occupied key is an alternate. Cancel
preserves both bindings; Confirm permits the normal native reassignment (which
can swap the old key onto the other action). The same action's existing key and
unused keys do not need confirmation. Native rejection rules still apply.

Ordinary `type: "key"` settings still use their own value storage. Their capture
now displays an inline vanilla `Binding` cell and commits on selected-key release;
the full-panel custom editor has been removed.

## Input handoff and evidence

Static inspection on 2026-09-17 checked installed Starfield **1.16.244.0** and
its matching Address Library with `addrlib_query.py`. Read-only decompilation
outputs are in `build/native-controls/` (local, ignored). Primary clearing and
the stricter conflict policy have **not** yet been verified in a fresh game session.

| Native operation | Address Library ID | RVA |
| --- | --- | --- |
| SettingsDataModel singleton | 939451 | 0x61EF3F8 |
| Dispatch one input event to BSInputEventUser | 124087 | 0x22DAE00 |
| Cancel native remap | 88684 | 0x15108E0 |
| Evaluate pending remap candidate | 88672 | 0x150E950 |
| Validate a single-context mapping | 124132 | 0x22EEA90 |

These bindings live in CommonLibSF's `RE::ID` namespaces, exposed through
`SettingsDataModel::GetSingleton()`, `SettingsDataModel::CancelRemap()` and
`BSInputEventUser::DispatchEvent()`. The model header describes the verified
base interfaces and allocation size; its internal UI data remains opaque.
Canonical evidence is recorded in OSF RE's `platform.control_map` and
`ui.menu_input` context modules.

The plugin hooks the evaluator's single-context validation call at `+0x185`,
checking its CALL target against the named CommonLibSF validator ID first. It
promotes only an allowed result with a different visible owner of the same key
pair to native confirmation. It never writes ControlMap records. The hook is
inactive outside OSF capture; other contexts and controller remaps keep vanilla
policy. A failed hook installation disables capture rather than allowing silent
conflict changes. Primary clearing needs no additional native hook.

`Main` input fanout (99438, RVA 0x1890C60) dispatches input to SettingsDataModel
only with Pause/Main Menu open. OSF now requires one of those menus underneath
it. `SettingsMenuRoute` checks the actual parent during `ProcessMessage(kShow)`.
If neither is open, it requests Pause and returns `kIgnore`, then queues OSF's
show after the Pause open event. The shortcut remains an enqueue-only input
callback. The existing Pause entry and direct OSF show requests use the same
gate; an existing Main Menu is also accepted. This keeps the current OSF movie
as a separate child menu, without adding a Main Menu button.

The parent remains open when Back closes OSF. If the parent closes first, the
route queues OSF hide; both capture owners are cancelled during hide/removal.
Duplicate pending requests coalesce and a pending Pause request expires after
five seconds. Rejected admission triggers native hide/removal cleanup too, so
only an admitted instance clears the route during that cleanup. The pending
request must outlive the initially rejected instance.

The forwarding fallback is retained until this parent route passes in-game
acceptance. During an OSF-owned remap, `NativeBindingEditor` gets the existing `SettingsDataModel`
singleton and converts it to its typed `BSInputEventSingleUser` base. The menu forwards
admitted events through the generic native dispatcher before its own input
handling. Native held-action admission and remapping remain intact. The receiver
cursor prevents duplicate delivery when Pause already processed that event.
OSF's menu hotkey handler is suppressed while this handoff is active, so a
captured binding cannot also request another OSF-declared menu.

The cancellation helper's unique Address Library entry and installed bytes were
checked. It clears the pending device/key pair and remapping/confirmation flags,
then publishes the non-remapping state through native 0x152B470. Its native call
at 0x1ECDAD1 and the inlined Pause cleanup at 0x1668A34 use the same sequence.
Escape, focus loss and menu removal release OSF's handoff and cancel the native
transaction. Successful completion only releases the handoff.

## Pending in-game acceptance

The production plugin and both movie variants compile. The focused
`osfsettings-binding-tests` target passes 23 host checks using an executable
six-argument CALL fixture, including cancellation/retry, both priorities,
native result preservation and behavior outside OSF capture. These checks do
not execute the game's clearing, popup or persistence implementations.

Use the test harness `SettingsSmoke` scenario when in-game testing is requested.
Check gameplay shortcut -> Pause -> OSF, existing Pause/Main -> OSF, Back to the
same parent, repeated shortcut requests, parent closure during capture, and a
failed/cancelled parent open. Verify the actual menu stack and native receiver
delivery before removing `NativeBindingEditor`'s forwarding fallback; retain
its capture ownership, shortcut suppression and cancellation responsibilities.
Then check normal/large text, mouse and keyboard slot selection, rebind and
old-key inactivity, repeated conflict accept/cancel on both slots, Escape/focus
loss/close during capture, primary/alternate clearing, vanilla Controls edits, and
restart persistence. A successful compile or matching deployment hashes do not
prove these behaviors.
Include immediate cancellation before the first remapping data update and both
UI threading modes when checking cancellation ordering.
