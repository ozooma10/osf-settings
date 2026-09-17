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
`menuBindings` maps native BasicMenuNav action names to their current keyboard
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
