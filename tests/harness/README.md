# In-game test instrumentation

This folder contains the Settings-side instrumentation used by the sibling
`OSF Test Harness` project's `SettingsSmoke` scenario.

- `TestHarness.h`: the small integration interface, with no-op functions for normal builds.
- `TestHarness.cpp`: cached input/UI observations and the snapshot export.
- `Paths.cpp`: configuration parsing and isolated test values storage.
- `Menu.cpp`: menu event observation and the test-only Scaleform callback.
- `MenuObservations.as`: private movie observation fields and methods, included inside
  `OSFSettingsMenu` so testing does not require exposing its private state.

XMake adds this folder's C++ implementations only with `--test_harness=y`.
The movie includes its observation fragment only with `CONFIG::testHarness` enabled.
Production files retain small calls at the existing observation points; the header
makes those calls no-ops in normal builds. Keep test logic in this folder.

## Translation registration checks

`TranslationRegistration.cpp` adds opt-in checks for Starfield 1.16.244.0.
The sibling runner's `-TranslationRegistration` switch writes
`"translationRegistration":true` to the private test configuration. Production and
test builds both register labels through CommonLibSF during the native constructor
resource load. There are no per-lookup translation overrides or alternate test hook.

The checks surround the production registration while its wrapper is unpublished.
They verify production schema labels before adding diagnostic labels, then test table
growth and duplicate replacement, registration replay, pooled reference ownership,
all generated keys, Unicode/control-character/long values, and preservation of every
pre-existing key/value identity. No map mutation occurs after publication.
`translationRegistration` in the exported snapshot contains cached startup results
and native manager checks from the existing menu callback. `ui.translations` comes
from real Flash text fields and native row clips. An additional private resource
load during construction tests merging; it is not proof of an in-session translator
reload. The extracted helpers have host coverage; the refactored production and
observation paths still require a fresh requested in-game run.

## Building and observations

Run the following commands from the repository root.

The development-only `test_harness` option adds passive input and menu observations.
It preserves normal hotkey activation.
Build the matching DLL and both movies into harness staging:

```powershell
$env:XSE_SF_MODS_PATH = 'C:\Modding\Starfield\OSF Test Harness\staging'
xmake f -y -m releasedbg --test_harness=y
xmake build 'OSF Settings'
```

The instrumented plugin requires `Data/SFSE/Plugins/OSFSettingsTestHarness.json`
with `{"valuesDir":"C:/Modding/Starfield/OSF Test Harness/state/settings-values"}`.
The harness prepares this directory and config. The path must be nonempty and
absolute; missing/invalid config prevents plugin initialization. This keeps tests
away from MO2's shared overwrite values. Normal builds ignore this configuration.

Only this build exports `uint32_t __cdecl OSFSettings_TestSnapshot(char*, uint32_t)`.
It returns the required UTF-8 buffer size including the NUL; insufficient capacity
leaves an empty buffer. Retry if the size changes. Zero means observation failed.
The export can be called from a worker: it copies cached observations and uses the
thread-safe settings service. It does not access engine-owned UI objects there.

`valuesDirectory` identifies the isolated store. `openKeyCode` comes from the actual main ControlMap binding sampled on the verified
game-thread drain. `testHotkeyCode` samples the main `learning/testHotkey` binding,
and `freeTestKey` selects an unused keyboard key from F6/F7/F8/F11 in MainGameplay
(255 means none). `ui.capture.saving` tracks the native save in progress.
`hotkeyInput` and `menuInput` record normal native callbacks and
include `sequence` and initial-down `pressSequence`. `learning` contains typed values
from the settings service. `ui` contains the selected mod/group/setting, visible row
identities and hit rectangles, key-capture state, and the AS3 frame counter. The movie
copies geometry on its own lane at 10 Hz; `observedAtMs` is a frame heartbeat and
`layoutObservedAtMs` is the age of the detailed snapshot. All `*AtMs` fields use the
Windows boot clock. Row rectangles use stage coordinates; map `stage.visibleRect`
to the game client area for mouse input. An empty `ui` after closing is intentional.
`menuBindings` maps native BasicMenuNav and VirtualController action names to their current keyboard
records (`keyCode`, `modifierKeyCode`, `slot`, `contextID`); use those records instead
of assuming WASD or arrows. Its `menuBindingsObservedAtMs` marks the same safe drain
sample. Slot 0 is main and slot 1 alternate; do not silently discard a chord modifier.
`ui.mouse` reports the actual Scaleform stage cursor. `ui.mouseDown` and
`ui.mouseClick` contain passive event sequence, frame, stage position, and target
ancestry, so an accepted Windows click can be distinguished from a delivered movie
click. The custom boolean row hides the vanilla checkbox clip; the row itself is
its normal mouse activation area.

For a normal build, configure `--test_harness=n` and rebuild;
the export and movie reporting code are absent. Never deploy an instrumented build
to the normal mod profile merely to run a test. See the sibling `OSF Test Harness`
project for session ownership, fixture isolation, and scenarios.

Its `SettingsSmoke -NativeBindings` mode checks standalone native capture,
conflict cancel/confirm, saving and persistence across a fresh game launch.
Native controls use the global Documents `ControlMap_Custom.txt`; this mode
backs it up, stops its owned sessions and restores the original bytes in cleanup.
It rejects `-KeepGame` so restoration cannot race a live native settings session.

`SettingsSmoke -Keybindings` adds the native root page. `ui.bindings` reports
snapshot state, search/source/key filters, selected slot and control rectangles.
`requiredActions` contains editable native rows with `bRequired`, allowing the
scenario to choose a real required action without modifying native definitions.
Row observations include action/source, numeric records and the required flag.
The scenario covers both sources, native clearing/validation, focus cancellation,
restart persistence and Pause entry. Physical controller input remains separate.
