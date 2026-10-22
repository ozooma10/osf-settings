# Performance workflow

Use the optional host profiler before changing production code:

```powershell
xmake f -y -m releasedbg --test_harness=n
xmake build -j4 osfsettings-performance
build/windows/x64/releasedbg/osfsettings-performance.exe FIXTURE_DIRECTORY OUTPUT_JSON
```

The executable links the same production service/store/localization/API sources
as the native suites. It is never installed with the mod. Each operation has one
first-use sample, five warmups, then 41 samples (11 for filesystem/catalog load).
Destruction is included. A separate untimed invocation counts ordinary C++
allocations and requested bytes on the measured thread. Aligned allocations,
engine allocators, live heap size, IO counts and CPU usage are not measured.
Idle checks use batches of 10,000 to exceed the clock resolution.

`tools/performance-fixtures.py OUTPUT_DIRECTORY INSTALLED_INVENTORY_JSON` creates
deterministic 10x40, 40x250 and 1x2000 fixtures, with one quarter enum controls and
the remainder booleans, English/Japanese overlays and no saved values. The
inventory is an ordered list of `{FullName: PATH}` records from enabled MO2 mods,
highest priority first, pointing at files below `SFSE/Plugins/OSF/Settings`.
Only schemas and translations should be inventoried; never copy player values.
An existing output directory is rejected. Runtime providers are measured using
complete default-value maps, replacement with the original registration token,
and unregister cleanup. Their in-memory save callback excludes disk IO.

Keep the original profiler executable, fixture files and hashes. Repeat original
and optimized executables in alternating fresh processes with no concurrent
build/game. The OS file cache is uncontrolled; first-use does not mean a cold
disk. Retain all samples and use multiple runs to identify noise. The recorded
host workload is not evidence that a real consumer polls every value each frame.

Runtime protocol and limitations are in [the harness guide](../tests/harness/README.md#performance-captures).
Build it with the sibling harness's `Test-Starfield.ps1 -Action Build -Scenario
SettingsSmoke`. Its configuration restore may select XMake's default `release`
mode: explicitly select `releasedbg` and `--test_harness=n` again before host
comparisons or production builds. Check the actual output path and hash rather
than reusing an executable left in another configuration directory.

For ordinary verification use `xmake test -j4`. The complete offline development
check is `pwsh tools/test-smoke.ps1`; it includes four movie preview variants,
release-contract fixtures and a production build in private staging. A clean
committed tree is required only for release packaging, not development testing.
Never deploy a test DLL, test movies or acceptance fixtures to the normal mod.

The measured optimization pass and evidence identities are recorded in
[PERFORMANCE_2026-10-22.md](PERFORMANCE_2026-10-22.md).
