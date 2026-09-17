# OSF Settings Slim

This is the primary Settings project for this workspace. `OSF Settings/` and
`OSF Settings old/` are historical/reference checkouts, not the default development targets.
Read `C:\Modding\Starfield\AGENTS.md` for shared workspace policy, then this
repository's `README.md` for its current build commands and integration details.

## Verification and in-game testing

Compile/typecheck by default. When the user requests in-game testing, read
`C:\Modding\Starfield\OSF Test Harness\README.md` and its `AGENTS.md`, then use
the `SettingsSmoke` scenario from that harness. It owns the isolated `OSF Testing`
profile, test builds, fixture save, screenshots, and reports. Add scenario checks
there when needed; keep Settings instrumentation in this repository's test build.

## Working with the user's edits

Inspect the current code and Git state before editing. The user may change code
between checkpoints to match their own stylistic preferences. Build on those
changes and preserve their formatting and simplifications. Ask the user before
reverting any of their changes, including stylistic changes.
