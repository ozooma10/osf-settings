# Inline native bindings

Hotkeys share the mod's settings groups. The optional declaration `group` selects
a declared group; omission appends to the first group, or an implicit General
group when no groups are declared. Unknown explicit groups are schema errors.

The shared `SettingsOptionList` owns selection and scrolling for all rows.
`NativeHotkeysList` embeds `Shared.Components.SystemPanels.SettingsControlListEntry`
from the imported `SettingsPanel.swf` / `SettingsPanel_LRG.swf` in each visible
hotkey row. The outer settings row draws the schema label and background; the
authored `Binding` cells retain their selected/listening states and native remap
events. Left/Right changes the selected binding slot when secondary bindings are
enabled. `RemapConfirmation` supplies vanilla's occupied-binding confirmation.
The shared group and keyboard selection passed the standalone runtime check
below; scrolling, mouse selection and large text remain separate acceptance cases.

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
outputs are in `build/native-controls/` (local, ignored). Primary clearing still
needs in-game verification; the main-slot conflict policy passed the check below.

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

The plugin hooks the evaluator's single-context validation call at `+0x185`. It
promotes only an allowed result with a different visible owner of the same key
pair to native confirmation. It never writes ControlMap records. The hook is
inactive outside OSF capture; other contexts and controller remaps keep vanilla
policy. A failed hook installation disables capture rather than allowing silent
conflict changes. Primary clearing needs no additional native hook.

`Main` input fanout (99438, RVA 0x1890C60) automatically dispatches input to
SettingsDataModel only with Pause/Main Menu open. OSF opens directly and uses
its own `kPausesGame` flag. During an OSF-owned remap, `NativeBindingEditor` gets
the existing `SettingsDataModel` singleton and converts it to its typed
`BSInputEventSingleUser` base. The menu forwards admitted events through the
generic native dispatcher before its own input
handling. Native held-action admission and remapping remain intact. The receiver
cursor prevents duplicate delivery when an already-open Pause menu processed
that event. There is no parent route, admission retry, event sink or open timeout.
Back closes OSF and returns to gameplay or the Pause menu it was opened from.
OSF's menu hotkey handler is suppressed while this handoff is active, so a
captured binding cannot also request another OSF-declared menu.

The cancellation helper's unique Address Library entry and installed bytes were
checked. It clears the pending device/key pair and remapping/confirmation flags,
then publishes the non-remapping state through native 0x152B470. Its native call
at 0x1ECDAD1 and the inlined Pause cleanup at 0x1668A34 use the same sequence.
Escape, focus loss and menu removal release OSF's handoff and cancel the native
transaction. Successful completion only releases the handoff.

## Verification

The production plugin and both movie variants compile. The hotkey lifecycle
target passes 17 host checks after removing the route fixtures. The current
`osfsettings-binding-tests` fixture stops at `reject a call to an unexpected
target`: it expects a target check that the existing editor no longer performs.
That implementation was left unchanged by the route removal. The remaining
binding fixture checks did not run; this host result is separate from the
successful native runtime checks below.

On 2026-09-17, `SettingsSmoke -NativeBindings` passed on Starfield 1.16.244.0
across two fresh game processes. It observed OSF open without Pause/Main, selected
the native row in the shared settings group, cancelled capture, saved an unused
key, cancelled and confirmed an occupied-key conflict, and observed the native
swap. Closing returned directly to gameplay. After restart, both bindings
persisted, the reassigned shortcut opened Settings, and capture still cancelled.
The original global `ControlMap_Custom.txt` was restored byte-for-byte afterward.
Report and screenshots: [20260917-194826-396-SettingsSmoke](../../OSF%20Test%20Harness/artifacts/20260917-194826-396-SettingsSmoke/result.json).

That run covers normal-text keyboard main-slot editing. Existing-Pause entry,
large text, scrolling, mouse/alternate-slot editing, primary/alternate clearing,
focus loss/menu removal during capture, and vanilla Controls edits remain
unverified by this run. Include immediate cancellation before the first remapping
data update and both UI threading modes when checking cancellation ordering.
