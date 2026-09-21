# Release acceptance

Evidence reviewed on 2026-09-20 (America/New_York); artifact IDs use UTC and
therefore begin `20260921`. The tested runtime is Starfield 1.16.244.0 (Steam),
using the sibling OSF Test Harness's isolated **OSF Testing** profile.
These are instrumented test builds, not the final release ZIP.

## Passing runtime checks

| Check | Passing report |
| --- | --- |
| Normal-text acceptance | [20260921-005857-787](../../OSF%20Test%20Harness/artifacts/20260921-005857-787-SettingsSmoke/report.md) |
| Large-text acceptance | [20260921-010539-537](../../OSF%20Test%20Harness/artifacts/20260921-010539-537-SettingsSmoke/report.md) |
| New game | [20260921-004025-603](../../OSF%20Test%20Harness/artifacts/20260921-004025-603-SettingsSmoke/report.md) |
| Cancelled load | [20260921-004150-473](../../OSF%20Test%20Harness/artifacts/20260921-004150-473-SettingsSmoke/report.md) |
| Normal-text keybindings | [20260921-004248-505](../../OSF%20Test%20Harness/artifacts/20260921-004248-505-SettingsSmoke/report.md) |
| Large-text keybindings | [20260921-004510-860](../../OSF%20Test%20Harness/artifacts/20260921-004510-860-SettingsSmoke/report.md) |
| Search input | [20260921-004731-767](../../OSF%20Test%20Harness/artifacts/20260921-004731-767-SettingsSmoke/report.md) |
| Translation registration and native bindings | [20260921-004831-687](../../OSF%20Test%20Harness/artifacts/20260921-004831-687-SettingsSmoke/report.md) |

Normal/large acceptance exercised actual Papyrus VM initialization, instance and
Global callbacks, duplicate registration, typed reads/writes, mixed-case and
UTF-8 round trips, registry snapshots, and menu/script synchronization. String
checks covered draft isolation, saving, Escape/focus cancellation, byte limits,
save-failure preservation and reopen persistence. Mod Issues checks covered live
report replacement, clearing, schema-less reports and long-detail scrolling.

Hotkey checks covered native/Papyrus delivery, held presses, blocking, console
suppression and capture cancellation. Repeated loads and Main Menu/Continue
checked retirement of outgoing registrations and continued settings operation.
The successful acceptance runs include normal window-close shutdown and exact
restoration of the original native Controls file. Values used the harness's
private store. Each report's directory retains `events.jsonl`, `builds.json`,
session identity, logs and screenshots; those local artifacts are not shipped.

## Limits of this evidence

- These are individual passing runs collected across retries. The last summary
  is an `acceptance-large` subset (`completeSuite: false`), not a single complete
  passing release-suite execution. Consult each run's build hashes when comparing
  it to later binaries; the tests span development iterations.
- Physical controller testing remains unverified. Mapped keyboard input and
  menu button prompts do not establish gamepad hardware behavior.
- Real failed-load recovery remains unverified. Attempts
  `20260921-005105-226`, `20260921-005430-352` and `20260921-005616-309` timed out
  waiting for the engine to begin and fail a named load. Cancelled-load coverage
  does not replace this case; these timeouts alone do not prove a product defect.
- The tested text examples do not establish every keyboard layout, IME or font
  glyph. The binding map represents US ANSI/MainGameplay PC mappings.
- Other game versions, every third-party UI combination, and third-party
  consumer compatibility are not established by these fixtures.
- A clean installation and upgrade-preservation check of the actual production
  ZIP remains a separate [packaging gate](PACKAGING.md#release-check).

## Repeating checks

When in-game testing is requested, use the sibling harness's
`Test-SettingsRelease.ps1` (read its README and AGENTS.md first). Its cases include
normal/large acceptance, new game, cancelled load, keybindings in both sizes,
search, translation registration and failed load. The harness owns profiles,
fixtures, builds and restoration. Do not distribute its ESM, scripts or test DLLs.
