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

The first offline candidate, `b786e93`, passed these checks on 2026-11-11.
Final candidate identity and current results belong in its preserved release record;
rerun the release validator after any committed source or documentation change.

- [x] All native suites pass, including dependency and extension-page coverage.
- [x] All four menu preview variants and preview-exporter regressions pass.
- [x] Smoke, release and preview-workflow contract checks pass.
- [x] Production ZIP, checksum and manifest pass payload and source checks.
- [x] Disposable reinstall preserves simulated settings and unrelated mod content.
- [ ] Review fresh normal/large-text preview captures.
- [ ] Full interactive game suite passes against the candidate source; document blocked cases honestly.

## Production acceptance

Use the exact production ZIP, not the instrumented game-test build.

- [ ] Clean mod-manager install opens from F10 and Pause; values persist after restart.
- [ ] Upgrade from 1.1.2 retains saved values, launcher history and custom/cleared bindings.
- [ ] Real consumer mod shows its settings, actions and hotkeys.
- [ ] Required shared-page host present/absent; optional host present/absent; ownership and values preserved.
- [ ] Missing/outdated dependency reports, Nexus link and malformed-schema guidance.
- [ ] Launchers on mod pages and external OSF UI handoff, including return/held-Accept behavior.
- [ ] Keybinding capture/conflicts, normal/large text, keyboard/mouse and Japanese localization.
- [ ] Physical controller verified, or retain its unverified notice in README.txt.
- [ ] New-game, cancelled-load and failed-load evidence reviewed; unresolved limitations recorded.

## Preserve and publish

- [ ] Preserve candidate ZIP, SHA-256, manifest, matching private DLL/PDB, logs and captures outside build/.
- [ ] Record release status, exact revision, artifact paths and any acceptance gaps.
- [ ] Obtain the user's instruction to publish; until then do not push, tag or create a release.
- [ ] After authorization, publish the validated revision and artifacts as v1.2.0.
