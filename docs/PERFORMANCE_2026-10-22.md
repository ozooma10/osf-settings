# Performance optimization — 22 October 2026

Implemented and locally deployed three focused changes. The largest verified
runtime improvement is **14.4% less native row-construction wall time** in the
synthetic 10,000-setting workload. The installed 25-setting workload was already
small; no perceptible benefit or FPS improvement is established there.

## Baseline and decisions

Started clean on `dev`, revision `c04d7ffe4a96dd56d62b49711b94f4933d062ce7`.
The optimization and validation pass preceded the subsequently requested local
commits. No branch change, push, tag or publication was made. CommonLibSF remained
at `42425651185684675b7d31d503f1b38eeb6753d3`. The separate harness remained clean
at `fb2628acbf06bf5a61f0b9261c9153144ef7c3d7`.

Windows 11, Ryzen 9 5900X, Starfield 1.16.244.0, Address Library v21,
XMake 3.0.9, MSVC 2026/C++23, `windows/x64/releasedbg`, nlohmann JSON 3.12.0,
spdlog 1.16.0, Flex 4.16.1, Temurin 8u462 and Ruffle 0.6.0. Starting configuration,
compiler command, dependency hashes and original deployed files are retained.

| Priority / measured cost | Change and ownership requirements |
| --- | --- |
| Repeated translated snapshots: 21.55 ms at 40x250 settings; 15.02 ms for one 2,000-setting page | `Localization.cpp` now walks live definitions and looks up their translations instead of rescanning the schema for every translated key. Catalog-load validation uses temporary indexes. Preserve fallback, errors, order, enum identities and provider schema replacement. |
| Dense provider snapshot: 117,871 C++ allocations | `SettingsService.cpp` constructs each detached copy with resolved values directly, avoiding an intermediate copied values map. Existing service mutex and callback/snapshot lifetime contracts remain. |
| Replacing a 2,000-setting provider: 7.08 ms | `SettingsStore.cpp` walks the validated, uniquely keyed new schema once. Preserve only still-valid old values, registration ownership, replacement order and failure behavior. |

No persistent cache, worker queue or engine-thread reassignment was introduced.
Startup still loads before readiness; public API and menu reads receive detached
snapshots on their calling lane. The existing SFSE dispatcher may run on several
threads. Engine operations added for testing use the harness's verified native
drain; movie observations remain in the movie callback. File persistence and
notification dispatch semantics are unchanged.

## Matched host results

Milliseconds below are the median of three runs' p50 values. Original and optimized
executables ran in alternating fresh processes, without a concurrent game/build.
Each operation had five warmups and 41 timed samples, including destruction;
catalog/file loads used 11 samples. Raw samples, first-call values, p95 and maxima
are retained. Allocation counting was a separate untimed invocation.

| Workload / operation | Before ms | After ms | Reduction |
| --- | ---: | ---: | ---: |
| Installed loose content: 5 mods / 25 settings, snapshot | 0.0071 | 0.0065 | Too small for a user-visible claim |
| Synthetic 10x40 settings, snapshot | 0.4559 | 0.3074 | 32.6% |
| Synthetic 40x250 settings, snapshot | 21.5528 | 9.0885 | 57.8% |
| Same dense registry, public registry read | 22.8911 | 10.5395 | 54.0% |
| One 2,000-setting page, snapshot | 15.0198 | 1.7544 | 88.3% |
| Same page, in-memory provider replacement | 7.0835 | 0.5641 | 92.0% |
| Same page, catalog construction/localization | 62.3033 | 49.7180 | 20.2% |

Dense provider snapshots allocate **87,911 instead of 117,871** ordinary C++
blocks, requesting **6,196,479 instead of 7,873,599 bytes** per invocation.
These are allocation traffic, not peak memory. Startup indexes add transient
allocations; catalog construction for 400 settings was essentially unchanged.

Synthetic fixtures use 25% enums, otherwise booleans, full English/Japanese
overlays and initially empty persisted values. Providers use complete maps and
an in-memory save callback. These figures exclude provider JSON registration
parsing and disk saves. Installed-content enumeration excludes archives and
runtime-only providers. First-call samples do not establish an OS-cold file cache.

## Fresh runtime evidence

Six completed background sessions used the same pinned disposable save, private
values, normal text size and English catalogs. Exact scene, INIs, mod lists,
seed/sidecar hashes, PID/start identity, binaries and UTC times are in each run.
Each ran ten seconds idle, eight menu open/close cycles, five provider transitions,
save/load with a fresh engine load event, and a final menu opening. No key/mouse
or focus command was issued. Snapshot readback verified all 30 provider states.

| Measured runtime operation | Baseline | Optimized |
| --- | ---: | ---: |
| 43 loaded schemas / 10,010 values, native `GetRows`, mean of 18 calls | 56.118 ms | 48.061 ms |
| Same operation, maximum | 59.518 ms | 50.261 ms |
| Dense service snapshot, mean across the same scenario's 47 calls | 14.053 ms | 7.733 ms |
| 13 schemas / 410 values, fresh control run `GetRows` mean | 1.923 ms | 1.911 ms |

The initial smaller optimized run averaged 2.015 ms versus 1.940 ms initially;
startup localization also rose from 4.014 to 11.280 ms. Retained original-DLL and
optimized-DLL control sessions yielded 1.923/1.911 ms for rows and 4.581/4.128 ms
for localization. These unfavorable observations are retained; no consistent
small-workload runtime speedup or startup regression is established.

Timings are inclusive elapsed time on the recorded calling threads, including
mutex acquisition, not CPU utilization. `GetRows` includes a snapshot: **do not
add those measurements**. Eighteen calls occurred across two movie-callback
threads and nine openings; their sum is not one game-thread stall. No frame-hook
interval or screenshot polling latency is presented as rendered-frame timing.
No work was moved off-thread. Fixed profiler slots reported zero drops; histogram
counts matched call counts with no overflow. Nested instrumentation affects its
parents; both builds used identical instrumentation. Fine lock waits, engine
allocation/IO counts, GPU timing and end-to-end menu latency were not measured.

Fresh game captures were inspected: the dense Home listing, counts and layout
rendered, and Settings reopened after load. One post-load image contains a normal
quest notification over the menu. This is limited visual acceptance, not physical
input or exhaustive editor acceptance.

## Validation and retained failures

- Baseline and final **24/24 native suites** passed. Final localization has 38
  checks; providers have 93. Added coverage exercises reordered/live schemas,
  changed types/options/confirmation requirements, future translation keys,
  replacement validation, unregister/re-register and detached snapshot ownership.
  Existing suites cover concurrent callbacks/unsubscribe, failed saves/retry,
  notification coalescing, lifecycle and binding invalidation.
- All **four preview variants** passed, retaining 156 fresh images; six native
  preview-exporter checks passed. Smoke-contract checks: 42 passed. Corrected
  release-contract fixtures: 86 passed. Production build and payload audit passed.
- The first host profiler compile lacked the registry definition include; fixed.
  The first harness build found a stale `count` widget reference; its test-only
  observation now returns an empty compatibility field. The first game run polled
  a UI field before initialization; the corrected predicate passed six later runs.
- An early host comparison copied the old `releasedbg` executable after the
  sibling build helper restored XMake's default `release` mode. That capture is
  excluded. `optimized-host-corrected` still ran beside a game and is exploratory;
  all quoted host results use `matched-host` with explicit configuration/hashes.
- The offline aggregate remains honestly marked failed in its original report:
  an outdated fake-game release fixture lacked the newer explicit interactive
  opt-in, cleanup dependency and scenario selection. Only the isolated self-test
  was corrected; its rerun passes 86 checks. The post-smoke source delta before
  this report was solely `tests/release_validation_tests.ps1`; compiled sources,
  movies and the passing native/preview stages did not change. No actual
  interactive game run was authorized or performed by that fixture.

## Deployment, evidence and limits

Production deployed to `C:\Modding\Starfield\MO2\mods\OSF Settings` under the
shared runner lock with Starfield closed. DLL, PDB and both production movies
were replaced; the other five files were already identical. All nine deployed
hashes match audited staging, and original files remain backed up. Production
payload contains the four public API export markers and no test exports,
profiler markers, test movies or acceptance fixtures. Configuration is restored
to `releasedbg; test_harness=n`; MO2 selection is restored to `Default`.

SHA-256 identities:

- Original deployed DLL: `67717234699DDD01278CD249E01DF66B9E5F764ABDFD676B0D9919168C20907C`
- Baseline instrumented DLL: `659B9C5E452108B6A5F97BEF39CE4DEE5D40CB3C81FCB650A232C3EA3C63463F`
- Optimized instrumented DLL: `612EAE9614F92FDC0554045A294CA02441D7341982DAE953BF1E69D194669B58`
- Deployed production DLL: `3C8B4E24D2CD80C0B95785E06C69712E72E5EAF5135BA57E36F7A8121C6E88A0`

Evidence root: `C:\Modding\Starfield\Artifacts\OSF Settings\performance-20261022-c04d7ff`.
See `matched-host/summary.json`, `runtime-summary.json`,
`runtime-evidence-validation.json`, `offline-final/result.json`,
`release-contracts-corrected.log`, `deployment.json`, and the source/binary/fixture
manifests there. Runtime captures/logs remain in the exact sibling harness paths
listed in `runtime-summary.json`; failed runs were retained too. Reproduction
commands and methodology are in [PERFORMANCE.md](PERFORMANCE.md).

All seven run-owned temporary MO2 profiles and seven temporary mods were removed;
the game is closed. Automatic approval review blocked deletion of the seven
isolated test-save directories. They remain under the Documents
`My Games/Starfield/OSFTestingFixture` directory; `save-retention.json` lists exact
paths and hash-verified evidence copies. Reusable harness staging is retained
separately from normal production deployment.

Remaining costs include native Scaleform row construction (about 48 ms/call at
10,010 values), repeated row requests during menu startup, and linear unsaved
value lookups in very large schemas. No proven redundant request was removed.
Uncontrolled OS-cold IO, physical keyboard/mouse/controller interaction, full
interactive release scenarios, new-game/cancelled/failed-load acceptance,
external UI handoff and a separate production-DLL runtime session remain untested.
No FPS, total CPU reduction or regression-free claim is made.
