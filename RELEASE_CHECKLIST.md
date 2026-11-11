# OSF Settings 1.2.0 release checklist

This checklist replaces the historical 1.0.0 launch checklist. A checkbox is
release evidence only when tied to the exact candidate and its retained report.
Publication is intentionally pending; preparing a candidate does not authorize it.

## Source and documentation

- [x] Project and installer metadata declare 1.2.0.
- [x] Release notes describe changes since 1.1.2 and preserve upgrade guidance.
- [x] Author guide covers versions, dependencies, Nexus IDs and required/optional shared pages.
- [x] Installation guide describes shared pages and metadata-based issue reporting.
- [x] Restore the release validation guide and document explicit interactive opt-in.
- [x] Candidate source committed and clean; record revision with preserved evidence.

## Automated validation

The 2026-11-11 follow-up passed 24 native suites (1,953 checks), four preview
variants (1,360 assertions), and production acceptance. The retained local record
is `C:/Modding/Starfield/Artifacts/OSF Settings/1.2.0-followup-20261111/RELEASE_RECORD.md`.
It carries the exact final candidate identity, runtime outcomes, captures and hashes.
Rerun the release validator after any committed source or documentation change.

- [x] All native suites pass, including dependency and extension-page coverage.
- [x] All four menu preview variants and preview-exporter regressions pass.
- [x] Smoke, release and preview-workflow contract checks pass.
- [x] Production ZIP, checksum and manifest pass payload and source checks.
- [x] Disposable reinstall preserves simulated settings and unrelated mod content.
- [x] Review fresh normal/large-text preview captures.
- [ ] Full interactive game suite passes against the candidate source; document blocked cases honestly.

The full-suite checkbox remains open: the failed-load injection aborts Starfield
1.16.244.0 with Settings absent as well. The harness blocks that unsupported
injection; recoverable failed-load handling remains unverified. Track the supported
cases separately in the retained runtime report, without claiming 13/13 passed.

## Production acceptance

Use the exact production ZIP, not the instrumented game-test build.

- [x] Clean mod-manager install opens from F10 and Pause; values persist after restart.
- [x] Upgrade from 1.1.2 retains saved values, launcher history and custom/cleared bindings.
- [x] Real consumer mod shows its settings, actions and hotkeys.
- [x] Required shared-page host present/absent; optional host present/absent; ownership and values preserved.
- [x] Missing/outdated dependency reports, Nexus link and malformed-schema guidance.
- [x] Launchers on mod pages and external OSF UI handoff, including return/held-Accept behavior.
- [x] Keybinding capture/conflicts, normal/large text, keyboard/mouse and Japanese localization.
- [x] Physical controller verified, or retain its unverified notice in README.txt.
- [x] New-game, cancelled-load and failed-load evidence reviewed; unresolved limitations recorded.

Production checks used exact ZIPs in disposable MO2 profiles, a separate public-SDK
consumer, and both OSF UI 2.1.0 backends. Japanese game checks found and verified
fixes for overridden INI language selection, multibyte placeholder substitution,
conflict-label overflow and large-text Home badge wrapping. Physical controller
input was not tested; the checked item above records the retained notice.

## Preserve and publish

- [x] Preserve candidate ZIP, SHA-256, manifest, matching private DLL/PDB, logs and captures outside build/.
- [x] Record release status, exact revision, artifact paths and any acceptance gaps.
- [ ] Obtain the user's instruction to publish; until then do not push, tag or create a release.
- [ ] After authorization, publish the validated revision and artifacts as v1.2.0.
