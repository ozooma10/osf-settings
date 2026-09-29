# Release validation

The validation self-tests check whether the release tools correctly accept evidence
and reject bad candidates. They do not establish that the mod works in Starfield.

## Commands and scope

Run from the Settings repository with PowerShell 7.2+:

```powershell
pwsh -NoProfile -File tests/smoke_bench_tests.ps1
pwsh -NoProfile -File tests/release_validation_tests.ps1
pwsh -NoProfile -File tools/test-smoke.ps1 -Plan
pwsh -NoProfile -File tools/test-release.ps1 -Plan
```

The release self-test requires the sibling `OSF Test Harness` checkout; use
`-Harness <path>` for another checkout. A missing harness blocks the contract
check instead of silently skipping it. Temporary Git repositories, simulated Data
and Documents folders, ZIPs, logs and receipts stay under `build/release-selftest`.
It does not launch Starfield, build a plugin, or edit a live mod-manager profile.

| Check | Useful evidence | Limit |
| --- | --- | --- |
| Archive validation | Exact payload allowlist, nonempty files, production configuration/markers, API name markers, basic JSON/PEX/SWF checks, checksum and sidecar | Synthetic DLLs are not loadable PE files; marker presence does not prove an exported function works or an ABI matches |
| Source identity | Release candidates require a clean commit; development smoke runs fingerprint actual tracked/untracked content and submodules | Ignored build outputs are excluded; staged rechecks cannot establish which source built them |
| Archive overlay | Re-extraction preserves simulated Documents settings/history/Controls and another mod's schema in Data | This is not migration, save/restart persistence, or MO2/Vortex installation acceptance |
| Suite coordination | Real coordinator, real Git/source helpers, fake game runner; build failures, failed/blocked cases, dirty/wrong checkout and source mutation are handled | Does not exercise game input, fixture preparation, or profile isolation |
| Scenario result publication | Real `Test-Starfield.ps1` publishes matching pass/fail/blocked results with simulated environment/game/report helpers | Does not verify the real environment or report collector |
| Receipt validation | Complete case inventory, strict pass flags, fresh build, matching source, unique result files, hashes, timestamps, scenario and exit-code agreement | Cannot compensate for a scenario whose assertions omit a feature |
| Native test reporting | Expected executed-test count, including rejection of zero/incomplete runs | Does not independently audit every native assertion |

## Audit findings addressed (2026-09-29)

- Removed stale Settings tests of a nonexistent private MO2 profile helper.
  Profile preparation belongs to harness acceptance; no replacement claim of
  profile safety is made by the packaging tests.
- Restored normal/large ordinary smoke cases and the working-tree/expected-project
  contract between the smoke runner and harness.
- Replaced report-directory guessing and exit-code-only acceptance with explicit
  per-invocation results. Missing, malformed, stale, inconsistent or changed
  results block acceptance. A skipped build is recorded and cannot satisfy the
  release/smoke caller's source-verified build requirement.
- Added rejection tests for missing/extra payloads, missing providers API marker,
  bad JSON, empty payloads, wrong configuration/revision and checksum sidecars.
- Corrected reinstall fixtures to use separate simulated Data and Documents trees.
- Made the release runner execute its tooling self-tests, check native execution
  count, and include normal/large Keybindings previews. Each preview requires its
  own fresh captures.
- Corrected the coverage map: action buttons, launcher execution and runtime
  providers/key observers do not yet have game scenarios. Native tests and preview
  assertions must not be presented as in-game acceptance.

## Running a candidate

`tools/test-smoke.ps1` supports uncommitted development work. It runs native suites,
four preview variants, tooling self-tests and a production build in private staging.
`-RunGame` adds the harness suite. It records source fingerprints and writes JSON,
Markdown and JUnit reports under `build/smoke`.

`tools/test-release.ps1` requires a clean committed Settings tree, builds a ZIP and
checksum, validates its payload and checks a disposable archive overlay. Reports
and captures go under `build/release-validation`. `-RunGame` adds instrumented
in-game checks from the same source; the production ZIP still requires manual
installation acceptance. The runner records these remaining gates even on success.

## Still requires acceptance

- Production ZIP: clean MO2/Vortex installs, upgrade, restart persistence and a
  real consumer mod.
- Action buttons: activation, confirmation/cancellation and success/failure/deferred
  completion through actual game UI and consumer callbacks.
- Launchers: native/external handoff, completion, failure, timeout, cancellation and
  recency with the current launcher API and OSF UI build.
- Runtime providers/key observers: real consumer persistence, notification and
  lifecycle behavior.
- Visual review, Japanese game UI, physical controller operation, and the manual
  lifecycle/keybinding gates in the release checklist.

The accepted text-editor focus-loss and Main Menu/Continue limitations remain
separate. A green tooling self-test does not close any of these game acceptance gates.
