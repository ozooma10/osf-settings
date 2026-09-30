# OSF Settings Test Mod

A separate consumer of the production OSF Settings public SDK. Enable **OSF Settings Test Mod** in MO2 and **OSFSettingsTestMod.esm** in its Plugins pane, alongside the current OSF Settings build. No instrumented Settings DLL or automated harness is required. The optional web panel also needs the matching OSF UI build.

Launch through SFSE, load a test save, open Settings with F10 or Pause → MOD SETTINGS, then select **OSF Settings Test Mod**. The second page, **OSF Test - Runtime provider**, is registered by the test DLL. Native hotkeys start unbound so the fixture does not replace your existing bindings: assign them in the test page or KEYBINDINGS.

The ESM contains only a start-enabled quest with a player alias. For an existing save, run **Initialize / re-register Papyrus fixture**, close Settings, wait for the notification, and reopen. Both Papyrus status rows should report successful registration. The alias registers again after save loads. Do not manually initialize after each load when checking whether automatic registration works.

Use a disposable save for the quest/script lifecycle checks. The fixture's settings and bindings follow normal cross-save persistence. Disable the mod and ESM when finished; keep your original save for ordinary play.

## Results you can observe

- **Read all values** displays the values actually read by the separate native plugin.
- **Show test counters** reports native hotkey presses, observed keys, value notifications, provider saves, action invocations and completed launcher handoffs. These counters reset when the process exits. Native hotkeys and observed keys also show HUD notifications.
- **Papyrus status / Global status** and their hotkey count rows show actual script callbacks. These rows are stored values, so an old Ready message alone is not proof that this session registered: edit a value or trigger an action/hotkey and check it changes.
- Native callback details and numeric API statuses are logged to `Documents/My Games/Starfield/SFSE/Logs/OSF Settings Test Mod.log`. Papyrus callbacks also use `Debug.Trace` if Papyrus logging is enabled.
- Deliberate failures are labeled EXPECTED TEST. They are test outcomes, not actual gameplay faults.

## In-game checklist

Repeat the normal menu checks with **Large text** off and on in OSF Settings. Search by both mod name and setting labels; scroll every group and inspect long labels/hints, focus, selection and footer prompts. Include keyboard/mouse and a physical controller.

| Area | Exercise | Expected result |
| --- | --- | --- |
| Values | Edit bool, negative/bounded int, slider, both enums and restart-marked toggle | Correct values and limits; restart notice shown; Native readback matches |
| Text | Save and cancel; empty string; mixed case and Japanese; 8-byte limit; hold Backspace; Alt+Tab | Saved text matches exactly, canceled draft discarded; byte limit enforced; record focus-loss behavior |
| Reset | Reset an individual value, then confirm Reset this test page | Defaults restored; other mods and provider values unaffected |
| Validation / registry | Run Check invalid API writes and Inspect public registry | Both report PASS; invalid writes do not alter the values |
| Persistence | Edit values, change saves, restart the game | Static values and provider values remain; native counters reset |
| Native hotkeys | Bind both callback hotkeys and menu hotkey; test keyboard, alternate slot, mouse where offered, and controller | One callback per press, no repeat while held; menu binding opens Inventory on release |
| Conflicts | Bind the second callback to an occupied key/button; cancel, then confirm; clear/reset | Correct conflict text and device; cancellation preserves bindings; restart preserves accepted binding |
| Key values | Rebind Observed key to keyboard and mouse; clear it; try clearing Required key | Observer tracks new binding; clearing stops it; Required key cannot be cleared; these are keyboard/mouse values, not controller hotkeys |
| Observer lifecycle | Toggle key observer off/on, press in gameplay | Counter stops while unsubscribed and resumes after resubscribe |
| Suppression | Try hotkeys with Settings, console, inventory, loading, and optional web panel open | Gameplay callbacks suppressed in blocked/menu states; resume after close |
| Explicit block | Start the 10-second block; close Settings immediately and press test hotkeys | No callbacks before the release HUD message; normal operation afterward. F10 is also blocked; Pause can reopen Settings |
| Notifications | Edit values, disable native subscription, edit again, re-enable | Change counter stops while unsubscribed; script observers remain independent |
| Native actions | Immediate success; cancel then accept confirmation; intentional failure | One invocation per activation; cancellation does not call handler; failure message shown |
| Deferred actions | Run 3-second action with Settings open; repeat and close before completion | Completion works in both cases; reopen and inspect counters/log |
| Duplicate completion | Run duplicate test | First result remains; log records rejected second completion |
| Provider edits | Edit all six provider types; restart | Values reload from the caller-owned file |
| Provider failure | Toggle save failure ON; edit provider value; toggle OFF and retry | Failed save preserves old value and emits no change notification; retry succeeds |
| Provider lifecycle | Edit values; replace definition; remove page; register again | Replacement retains valid values and adds a field; removal hides page; re-register restores saved values |
| Provider key observer | Bind Provider observed key; press in gameplay; remove provider and press; register again and press | Provider observer counter increments only while its definition exists; saved binding and observation resume after registration |
| Issues | Report; inspect long detail through paragraph 12; replace warning; clear one; clear all | Error/warning styling, scrolling, replacement and clearing work; schema-less mod appears |
| Native launcher | Open TEST: schema → Inventory from gameplay and Pause | Settings/Pause hand off to Inventory; closing restores normal input; recency updates |
| Callback launcher | Open TEST: callback → Inventory | After-close counter increments once; Inventory appears |
| Delayed / cancellation | Open 5-second launcher; allow completion, then repeat and cancel during wait | Success hands off; canceled request never opens Inventory; late completion logs nonzero status |
| Rejection | Open rejected launcher | Intentional reason shown; Settings stays open |
| Timeout / late completion | Run 30-second timeout and then 35-second late-completion case | Settings recovers at its 30-second deadline; late callback cannot open Inventory |
| Availability | Inspect missing native-menu card; toggle availability of toggle card | Missing menu stays unavailable; toggle card follows its current availability/reason |
| Papyrus actions | Initialize if needed; immediate alias, deferred Global, typed read/write, reset | Actual script completion; typed test writes false / 7 / 0.65 / quiet / Papyrus typed values; Native readback agrees |
| Papyrus hotkeys / notifications | Bind alias and Global hotkeys; edit native values | Correct script counter/status changes; one activation per press |
| Papyrus issues | Report and clear script warning | Script-owned issue appears and disappears |
| Save lifecycle | Save/reload, load a different save, new game; return to Main Menu/Continue; cancel a load | Check registrations and hotkey suppression/recovery without manual reinitialization; record failures and accepted framework limitations separately |
| External OSF UI | Open TEST: OSF UI handoff first time and again | Visible panel on cold/warm opens; input, select, drag and hover work; Escape/Back dismiss select before closing; gameplay input returns |
| Japanese | Restart with Japanese game language | Translated title/group/value/action/hotkey text renders; untranslated fields fall back; IDs and saved values unchanged |
| Production install | Install/reinstall the actual OSF Settings ZIP with this consumer enabled | Test mod's files remain separate; static/provider values and Controls survive; no test plugin or fixture in production package |

Timeouts are real-time. Native delayed jobs continue while Settings pauses the game. Papyrus uses `Utility.WaitMenuPause`; bootstrap or other VM work may need gameplay to resume.

The fixture exposes manual cases; it does not certify them automatically. Failed-load recovery needs an independently reproducible load failure. Do not deliberately damage a normal save. Framework-wide failure recovery, supported legacy consumers and mod-manager upgrade acceptance still require their corresponding real scenarios. The external panel tests the current OSF UI integration, not legacy migration.

## Files and removal

- Static values: `Documents/My Games/Starfield/OSF/Settings/osfsettings-test.json`.
- Provider values: `Documents/My Games/Starfield/OSF/SettingsTestMod/provider.json` (owned and atomically written by this plugin).
- Test hotkeys: the game's `ControlMap_Custom.txt`; launcher recency uses the framework's shared `internal.json`.
- Do not delete shared Controls, launcher history or the OSF directory to remove the test mod. Disable its MO2 entry and ESM; optionally remove only its two value files if you want fresh fixture defaults later.

## Rebuild and deploy

From the OSF Settings repository:

```powershell
pwsh -NoProfile -File examples/acceptance/build.ps1 -Deploy
```

The opt-in target `osfsettings-acceptance-mod` builds the consumer DLL; the helper compiles the scripts and generates the quest ESM. Output stages under `build/acceptance-mod/Data`; deployment copies only owned files to `$env:XSE_SF_MODS_PATH/OSF Settings Test Mod`, preserving `meta.ini` and unrelated files. SHA-256 verification is recorded in `build/acceptance-mod/payload-sha256.json`. It does not enable mods, edit profiles, launch Starfield or include this fixture in the production release ZIP.
