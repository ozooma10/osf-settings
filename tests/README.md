# Test workflow

Run from the repository root with the existing XMake configuration. XMake builds
selected tests before running them, so a separate build command is unnecessary.

```powershell
# All 21 native suites, including SDK, service and key coverage.
xmake test -j4

# One area, or several related suites in one invocation.
xmake test -j4 -g tests/settings
xmake test -j4 'osfsettings-schema-tests/*' 'osfsettings-store-tests/*' 'osfsettings-registry-tests/*'
xmake test -j4 'osfsettings-launcher-tests/*'
```

| Group | Coverage |
| --- | --- |
| `tests/settings` | Localization, schema, store, service, SDK, strings, registry |
| `tests/input` | Keys, registration, input routing, lifecycle, binding editor/menu/snapshots, blocks, callbacks |
| `tests/launcher` | Launcher registry and recency |
| `tests/actions` | Action registration, dispatch and completion |
| `tests/diagnostics` | Issues, diagnostics service and API |
| `tests/papyrus` | Papyrus values, actions and subscriptions |

Keep build outputs warm. Four compilation jobs worked on the profiling host;
avoid forced rebuilds or serial builds unless investigating a concrete issue.
Shared build/API changes warrant the whole suite; ordinary local changes should
select their relevant coverage once. A passing unchanged rerun adds little evidence.

## Shared compilation and coverage

`osfsettings-test-core` compiles settings, persistence and diagnostics objects once.
`osfsettings-test-services` compiles compatible engine-facing services once, using
one foundation PCH. `osfsettings-test-tasks` shares the standalone task wrapper.
Each fixture remains a separate executable with its own stubs and process state.
Sources included directly in fixtures, the lifecycle harness definition, and the
callback suite's SFSE stub retain their separate compilation contexts.

The `osfsettings.localization` rule runs `tools/generate-localization.py` before
building the plugin and the test core library. It validates referenced keys and
writes `build/generated/English.{h,as}`, rewriting only outputs whose content changed.

The historical `osfsettings-tests` aggregate used the removed `HotkeyService` API.
Its still-supported SDK, service and key checks now have standalone targets.
The store suite runs its previously excluded store section; generic schema
validation lives in the schema suite. The old
hotkey fixture described removed subscription/unregister/suppression semantics;
current registration, callback, block, lifecycle and Papyrus suites cover the
supported contract. No compatibility implementation was added for the removed API.

Binding-snapshot checks exercise both queued and inline `TaskQueue::AddTask`
execution, missing maps/contexts, invalidation and lifetime handling. They follow
CommonLib's current task contract; they do not establish in-game threading safety.
Preview fixtures explicitly mark required keys with `allowUnbound: false`.

## Menu previews

```powershell
pwsh -NoProfile -File tools/test-menu-preview.ps1
pwsh -NoProfile -File tools/test-menu-preview.ps1 -LargeText
pwsh -NoProfile -File tools/test-menu-preview.ps1 -Bindings
pwsh -NoProfile -File tools/test-menu-preview.ps1 -LargeText -Bindings

# Same checks, with rendered PNGs and the original visual settling interval.
pwsh -NoProfile -File tools/test-menu-preview.ps1 -Capture
pwsh -NoProfile -File tools/test-menu-preview.ps1 -LargeText -Capture
```

Routine checks skip screenshot encoding, advance on rendered frames and expected
asynchronous results, and fail a stalled step after two seconds. All behavioral
assertions remain enabled. The runner reads appended log data every 50 ms.
`-Capture` writes `build/preview/normal-*.png` or `large-*.png`; ordinary runs do not
refresh old captures. Do not run previews concurrently: they share prepared assets.

Only the requested movie variant is compiled. Movie/host builds and prepared
assets reuse content fingerprints of authored inputs and output hashes. External
toolchains and the large game archive use path/size/time metadata. Use `-Force`
after replacing those external files while preserving their metadata, or to
diagnose a cache. Missing or changed outputs rebuild automatically.

Preview movies live in `build/scaleform/preview`, separate from the production
movies in `build/scaleform`. `tools/build-scaleform.ps1` defaults to both production
variants and also accepts `-Variant Normal`, `-Variant Large`, and `-Force`.
Preview, large-text and harness flags participate in movie fingerprints.

## Game acceptance

The [final validation runner](../docs/FINAL_VALIDATION.md) combines native and
preview checks, production archive verification, disposable reinstall checks,
and the source-matched runtime suite. `tools/test-release.ps1 -RunGame` explicitly
enables game launches; omitting that switch runs only the offline stages.
`pwsh -NoProfile -File tests/release_validation_tests.ps1` checks the runner's
failure handling with fake game results and malformed archives, without launching.

Native assertions and Ruffle previews do not replace input, controller, lifecycle
or menu-handoff acceptance in Starfield. When requested, use the sibling
`OSF Test Harness/Test-SettingsRelease.ps1 -Cases ...` runner. It already builds
once and reuses that build across selected cases. `-SkipBuild` requires current
harness artifacts; retain fresh sessions for lifecycle scenarios. Game waits were
not shortened as part of the native/preview optimizations.

See [performance measurements](../docs/TEST_PERFORMANCE.md) for the baseline and
verified results.
