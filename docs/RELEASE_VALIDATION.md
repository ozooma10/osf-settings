# Release validation

Run from the OSF Settings repository with PowerShell 7.2+, XMake and the
dependencies installed by `tools/setup.ps1 -Preview`. The sibling
`OSF Test Harness` checkout owns game orchestration. All game tests run locally.

## Development checks

```powershell
pwsh -NoProfile -File tools/test-smoke.ps1
```

This accepts a dirty working tree, records its source-content identity, and runs
coverage-map checks, native suites, four captured menu previews, preview exporter
regressions, workflow/release contract tests and a production build in private
staging. It rejects source changes during the run. It does not package a release.

## Release candidate

Commit source and documentation first. Packaging and release validation reject
uncommitted or untracked files, including dirty submodules.

```powershell
pwsh -NoProfile -File tools/test-release.ps1 -Plan
pwsh -NoProfile -File tools/test-release.ps1 -Label rc1
```

The offline release pass runs native, workflow and preview checks, builds a
production ZIP, verifies its checksum, exact payload allowlist and production API
markers, and checks simulated reinstall preservation. The final source revision
must still match the candidate manifest. No live MO2 installation is overwritten
by packaging. No command above publishes, tags or pushes a release.

Use `-Manifest <manifest.json>` to verify an existing candidate at the same clean
commit. Use `-ForceVerification` when fresh preview/contract execution is needed
instead of content-verified cached results. Native tests still run each time.

Reports are in `build/release-validation/<run>/result.json` and `report.md`;
archives, SHA-256 sidecars and source manifests are in `build/packages/<run>`.
Each run retains logs and screenshots. `scope: offline` and `offline-passed`
leave all runtime acceptance outstanding. Exit 0 means requested stages passed,
1 indicates a test failure, and 2 indicates a blocked run/infrastructure problem.
Inspect stage results as well as the exit code.

## Interactive game acceptance

The game suite sends keyboard/mouse input and requires explicit interactive-test
authorization and an idle desktop. Do not run alongside another game test or a
user-owned Starfield session. `-RunGame` alone is rejected before any work begins.

```powershell
pwsh -NoProfile -File tools/test-release.ps1 -RunGame -Interactive -Manifest <manifest.json>
# For development sources instead:
pwsh -NoProfile -File tools/test-smoke.ps1 -RunGame -Interactive
```

The harness owns the current case list; inspect `-Plan` rather than copying that
list into a second runner. It builds an instrumented candidate in private staging,
uses disposable profiles/values/saves, and records fresh process, source and result
identities. A missing, stale, contradictory or partial receipt cannot establish
a complete suite. Stop and investigate blocked cases; never relabel them passed.

On Starfield 1.16.244.0, the locked-save failed-load injection also aborts with
OSF Settings absent. The harness blocks that injection before mutating a save;
it cannot establish recovery from a recoverable engine load failure. Retain the
blocked receipt and compare supported case results separately. The packaged
README records this unverified boundary.

## Acceptance beyond automated checks

- Review retained normal/large-text screenshots, including Mod Issues, dependency
  cards, contributed settings and launcher buttons.
- Install the unmodified production ZIP with a mod manager. Check opening,
  persistence across restart and upgrading without losing values or bindings.
- Exercise a real consumer and external OSF UI handoff with production binaries.
- Check required/optional shared-page hosts present and absent, dependency version
  warnings, and the source attribution of contributed controls.
- Test physical controller input separately or retain the unverified notice.
- Track focus-loss/editor behavior and new-game, cancelled-load and failed-load
  results separately. A failed fault-injection setup does not prove recovery.

Native tests and previews do not prove game behavior. Instrumented runtime tests
do not prove the production ZIP works in a real installation. Before publishing,
preserve the exact archive, checksum, manifest, matching private DLL/PDB and all
evidence outside disposable `build/`, and record any accepted gaps. See the
[release checklist](../RELEASE_CHECKLIST.md).
