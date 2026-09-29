# Test workflow

Run from the repository root with the existing XMake configuration. XMake builds
selected tests before running them, so a separate build command is unnecessary.

```powershell
# All 22 native suites, including SDK, service and key coverage.
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
one foundation PCH.
Each fixture remains a separate executable with its own stubs and process state.
Sources included directly in fixtures, the lifecycle harness definition, and the
engine-independent callback suite retain their separate compilation contexts.

The `osfsettings.localization` rule runs `tools/generate-localization.py` before
building the test core library. Plugin builds generate the same files through
`tools/build-scaleform.ps1`, so they need no second generator invocation. The generator
validates referenced keys and writes `build/generated/English.{h,as}`, rewriting
only outputs whose content changed.

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

Install the optional preview dependency once with
`pwsh -NoProfile -File tools/setup.ps1 -Preview`.

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

For a single automated development entrypoint, including uncommitted changes,
use `pwsh -NoProfile -File tools/test-smoke.ps1 -RunGame`. The
[release validation guide](../docs/RELEASE_VALIDATION.md) describes coverage, isolation, report
formats and remaining acceptance gaps. Omit `-RunGame` for an offline run.

The [release runner](../tools/test-release.ps1) combines native and preview
checks, production archive verification, disposable reinstall checks, and a
clean-commit check. Commit first: uncommitted or untracked files block packaging
and validation. Run `pwsh -NoProfile -File tools/test-release.ps1` for the
offline pass. Reports and screenshots go under `build/release-validation`; exit
code 0 means every requested stage passed, and the report's `scope` says whether
the in-game suite was part of it. `-Plan` lists the stages and the in-game cases;
that case list is read from the sibling harness's `Test-SettingsRelease.ps1 -Plan`,
so this repo never carries its own copy.

Native assertions and Ruffle previews do not replace input, controller, lifecycle
or menu-handoff acceptance in Starfield. Add `-RunGame` to run the harness suite
as the `runtime` stage (`-Harness` points at a checkout other than the sibling
`OSF Test Harness`). The suite receives `-ResultPath` and writes its JSON summary
there; the stage requires a fresh build of the packaged commit and matching source
content, every advertised case exactly once, and a fresh, hashed result per case.
Missing, stale, changed or contradictory evidence blocks acceptance.
Focused rechecks use the harness directly, `Test-SettingsRelease.ps1 -Cases ...`;
a filtered summary is never a complete suite. Game orchestration and its results
are owned by that project.

`pwsh -NoProfile -File tests/release_validation_tests.ps1` checks malformed-archive
rejection and archive overlay behavior with synthetic fixtures. It requires the
sibling harness (or `-Harness`) and runs its real suite coordinator in a disposable
tree with a fake game runner. A second fixture checks the real single-run
entrypoint's result publication with simulated environment/game helpers.
No game is launched. These checks and `tests/smoke_bench_tests.ps1` also run as the
release runner's first validation stage. See the guide for the audit and limits.
