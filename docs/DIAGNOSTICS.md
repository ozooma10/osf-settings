# Issue reporting

Report detected problems in the menu's **Mod Issues** tab. This C++ API needs no
settings schema and has no Papyrus equivalent.

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

- `modId` follows the [schema ID rules](SETTINGS.md). `id` is a stable,
  case-sensitive issue ID within your mod. ID and title must contain nonblank text.
- Reporting the same `(modId, id)` replaces the whole report. `impact` and
  `nextSteps` are optional; omitting them clears their previous text.
- Severity defaults to `Warning`. Use `Error` for a failed operation or unavailable
  feature. Text is NUL-terminated UTF-8 and is copied before `Report` returns.
- `Clear` removes one issue; `ClearMod(modId)` removes all of your mod's issues.
  Both succeed if the issues are already absent.
- Reports last for the game process. Recheck conditions after relevant changes
  and clear resolved issues; opening Settings does not run checks for you.
- Initialize the client before sharing it. Report/clear calls are synchronous
  and thread-safe. Check `Status`: `Ok`, `NotReady`, `InvalidArgument`, or `InternalError`.

See the [SDK header](../sdk/OSFSettings_Diagnostics.h) and
[buildable example](../examples/diagnostics/README.md).
