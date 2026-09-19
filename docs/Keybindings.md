# Keybindings

KEYBINDINGS sits between ALL MODS and MOD ISSUES. It includes native
MainGameplay PC actions and explicitly registered OSF hotkeys. Ordinary schema
`type: "key"` settings and per-mod hotkey rows remain on their mod pages.

Click a keyboard key to filter by the primary key or a chord modifier; click it
again to clear that filter. Search matches action, source, and key names. The
source selector cycles All, Game, and individual mods. Clear Filters resets all
three filters. Mouse and unsupported keyboard codes remain in the action list.
Enter and numpad Enter highlight together because the native code is shared.

Select an action with native Up/Down events and a slot with Left/Right, or click
its native binding cell. Search filters live: Enter returns to results,
and Escape closes Settings, including while search is focused. Capture/confirmation/save freeze navigation and
filters. Back closes Settings from the page. No per-action reset calls the
game's global Controls reset.

Grey, orange, and split key markers distinguish Game, Mod, and mixed ownership;
a diamond marks multiple action owners. Exact device/key/modifier assignments
across different actions are labelled **potential conflicts**. Two slots of one
action do not conflict with each other. Classification runs before filtering;
native validation remains authoritative.

## Ownership and lifecycle

`BindingSnapshot` copies MainGameplay keyboard/mouse mappings only on the
verified `BSService::TaskQueue` drain. It refuses disabled queues and wrong-thread
inline callbacks. The synchronized mailbox owns strings and numeric fields;
queued work holds a weak mailbox reference, never a menu or mapping pointer.
Request generations reject stale results after refresh/close. Missing data has
an explicit loading/unavailable state and cannot become an editable unbound row.

The movie joins context/action identities to native `ControlBindingsData` PC
rows and explicitly registered metadata. Native labels, flags, slots and glyph
objects survive the join. `NativeHotkeysList` and `NativeBindingEditor` own the
single capture, conflict, validation and save transaction for both sources.
The public SDK, schema and persistence formats are unchanged.

Search uses `OSFSettingsMenu::WantsMovieEventForward` to admit character events
only while editing. Other events use the base implementation, except disabled
keyboard `Activate` events: the native forwarder would synthesize Enter from
them even though the text context disabled the action. Characters arrive
separately, so suppressing that button preserves the typed E.

The existing guarded queue switches the requested contexts between `kTextInput`
and the normal navigation contexts, retaining weak generations and a fresh menu
lookup. This prevents E/Accept and WASD/navigation mappings from affecting text.
Priority matches PauseMenu (`0x0B`): native dynamic context updates exclude
priorities `0x0C` and above. `IMenu::kUsesMenuMode` uses the engine's menu-mode lifecycle,
whose counter suppresses console-command hotkeys. Removal invalidates queued
requests; the engine removes applied contexts and menu-mode state on close.
No ControlMap owner-table offsets, insert/find/erase relocations, or local
text-owner release path remain. The existing CommonLib layouts and Address
Library dependencies remain; this is not proof of another game version.

Console itself uses a focused command field and a dedicated Console context;
its native show/hide handler still inserts/removes its name in the owner table.
It inherits the base character gate. Copying that lifecycle would recreate the
removed adapter. The separate vanilla `TextInputMenu` publishes a submitted
`sText` or cancellation, with no per-edit result event in its movie contract;
using it would replace inline live filtering with a submit/cancel dialog.
No generic native acquire/release wrapper for an arbitrary custom menu was
found in the inspected paths. Native evidence, callers, and addresses are in
`OSF RE/tools/ghidra/context_repo/modules/ui.menu_input.json`.

`MenuFocus` records Windows foreground changes on a menu-owned observer thread
with a message loop. It publishes only an atomic loss flag. Cancellation occurs
on the menu's input/movie lane, including after the game resumes from being
minimized. It does not call engine functions from the observer. The thread
unhooks and joins at destruction, following the
[WinEvent callback delivery contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook).

## Verification

The native snapshot tests exercise generations, owned copies, disabled/inline
queue guards, missing context, invalidation, and destroyed-menu work. Existing
binding/editor tests remain applicable. Preview fixtures cover mixed ownership,
exact overlaps, shared modifiers, alternate slots, unbound and mouse-zero
bindings, long/unsupported labels, scrolling, focus and stale results.

`tools/test-menu-preview.ps1 -Bindings [-LargeText]` runs the focused fixture;
omitting `-Bindings` also checks ordinary settings. Previews simulate data and
layout, not the engine binding transaction. They compile with `CONFIG::preview`;
the production build excludes their label and Ruffle popup-placement adapters.
The preview approximates native `TextFieldEx` shrinking for binding cells, which
Ruffle does not implement; production retains the stock glyph/text implementation.

Live checks run in the sibling OSF Test Harness's isolated OSF Testing profile:
`Test-Starfield.ps1 -Scenario SettingsSmoke -Keybindings`. Its report retains
build hashes, WGC screenshots, observations, and exact global Controls backup /
restore proof. Physical controller input remains a separate acceptance check;
mapped keyboard navigation is not controller hardware proof.

## Input simplification validation, 2026-09-18

The isolated `SettingsSmoke -Keybindings` run
`20260918-182115-572-SettingsSmoke` passed 47 assertions over two fresh game
processes. It covered actual E/WASD/punctuation input, editing keys, focus loss,
Escape closing active search, reopening, Enter returning to filtered results,
native capture/save/conflicts/clearing/validation, Pause entry and restart
persistence. See the [runtime report](../../OSF%20Test%20Harness/artifacts/20260918-182115-572-SettingsSmoke/report.md).
The game was stopped and the exact original Controls file restored (the same
SHA-256 recorded below). Earlier failed and blocked reports are retained.

The normal DLL and both SWFs subsequently built and installed successfully.
Installed hashes match the outputs and the DLL has no test snapshot export;
see [production manifest](../build/text-input-production.json).
Live coverage used the isolated test build, normal text, and keyboard/mouse.
Physical controller input and console-command-hotkey suppression were not
established by this run; the latter has static native evidence only.

## Windows handoff, 2026-09-18

Production native compilation and both SWFs passed with `--test_harness=n`.
The DLL and movies are installed in `MO2/mods/OSF Settings Slim`. The production
DLL export table contains no `OSFSettings_TestSnapshot` entry. The existing
XMake warning for the absent `src/Input/HotkeyService.cpp` in the unrelated
`osfsettings-tests` target remains unchanged.
All three installed hashes match their build outputs; the local manifest is
[`build/keybindings-production.json`](../build/keybindings-production.json).

Focused host checks passed: schema 71, registration 19, binding editor 31,
native binding presentation 22, snapshot lifecycle 14 (157 total).
Full preview checks pass for normal and large text (69 each), including existing settings
and the new fixtures. Captures and logs are in `build/preview`.

The isolated live run `20260918-044850-835-SettingsSmoke` passed 39 assertions
across two fresh game processes. It verified gameplay and Pause entry, mapped
navigation and alternate selection, search shortcut isolation, mouse capture,
vanilla/OSF saving, conflict cancellation and confirmed swaps, optional clearing,
native required-binding rejection and restoration, focus-loss cancellation,
closing/reopening, and persistence after restart. Its report is
`../OSF Test Harness/artifacts/20260918-044850-835-SettingsSmoke/report.md` from
the repository root. Earlier failed runs remain in that artifact directory;
they exposed text ownership/focus handling and harness assumptions corrected
before this passing run.

The game was stopped and original global `ControlMap_Custom.txt` restored with
SHA-256 `AE68A47946F9374B8E472C5AE42A30F92ACFC444EE58DE761CFCDF654DE1F9CD`.
Settings values stayed under the harness's private `state/settings-values`.
Normal-profile settings and schemas were not edited.

Outstanding acceptance: a physical controller. All four Windows XInput slots
returned `ERROR_DEVICE_NOT_CONNECTED` (1167). Native event handling and keyboard
navigation are covered, but do not establish controller hardware behavior.
Live rendering used normal text; large text has offline visual verification.
