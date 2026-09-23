# OSF Settings Slim

Read `README.md` and `tests/README.md` for build and validation commands.

- Preserve the existing working tree and its staged changes. Commit only on request.
- For native validation, use one focused `xmake test -j4` command with a group or
  multiple test patterns. It builds selected tests automatically; do not precede
  it with a separate test build or invoke each suite separately.
- Use `xmake test -j4` for shared build/API changes that need all 21 suites.
  Keep warm outputs and the existing configuration. Avoid routine clean/rebuild
  flags, mode switches, and `-j1`; retry serially only for an actual build failure.
- Menu behavior checks use `pwsh -NoProfile -File tools/test-menu-preview.ps1`
  and `-LargeText` as relevant. Add `-Capture` when inspecting rendered output;
  `-Force` deliberately bypasses preview caches.
- Native and Ruffle results are separate from game acceptance. Launch the game
  only when requested. When game testing is requested, select relevant cases in
  the sibling `OSF Test Harness/Test-SettingsRelease.ps1`; it builds once for the
  selected cases. Use `-SkipBuild` only when its harness artifacts are current.

Do not weaken assertions or shorten game readiness checks to improve timings.
