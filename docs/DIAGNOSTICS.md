# Issue reporting

Report detected problems in the menu's **Mod Issues** tab.

## Papyrus

```papyrus
Bool reported = OSFSettings.ReportIssue("mymod", "missing-assets", "Custom animations are unavailable", true, "Characters use default animations.", "Install this mod's animation pack.")

; After confirming recovery:
Bool cleared = OSFSettings.ClearIssue("mymod", "missing-assets")
; Or remove every report owned by this mod:
Bool clearedAll = OSFSettings.ClearModIssues("mymod")
```

`ReportIssue(modId, issueId, title, isError = false, impact = "", nextSteps = "")` returns `true` when accepted.
`isError = false` reports a warning; `true` reports an error. The two clear calls return `true` for valid IDs even if no matching report exists.

## C++

```cpp
#include "OSFSettings_Diagnostics.h"

OSFSettings::API::Diagnostics::Client diagnostics;

// During SFSE kPostPostLoad:
if (diagnostics.Init()) {
    auto status = diagnostics.Report({
        .modId = "mymod",
        .id = "missing-assets",
        .severity = OSFSettings::API::Diagnostics::Severity::Error,
        .title = "Custom animations are unavailable",
        .impact = "Characters use default animations.",
        .nextSteps = "Install this mod's animation pack."
    });
    // Check status == OSFSettings::API::Status::Ok.
}

// After confirming recovery:
auto status = diagnostics.Clear("mymod", "missing-assets");
```

- `modId` follows the [schema ID rules](SETTINGS.md#schema-fields-and-types). `id` is a stable, case-sensitive issue ID within your mod. ID and title must contain nonblank text.
- Reporting the same `(modId, id)` replaces the whole report. `impact` and `nextSteps` are optional; omitting them clears their previous text.
- Severity defaults to `Warning`. Use `Error` for a failed operation or unavailable feature.
- `Clear` removes one issue; `ClearMod(modId)` removes all of your mod's issues. Both succeed if the issues are already absent.
- Reports last for the game process. Recheck conditions after relevant changes and clear resolved issues; opening Settings does not run checks for you.

See the [SDK header](../sdk/OSFSettings_Diagnostics.h) and the [example plugin](../examples/diagnostics/main.cpp).
