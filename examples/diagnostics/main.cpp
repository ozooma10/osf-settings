#include <SFSE/SFSE.h>

#include "OSFSettings_Diagnostics.h"

namespace
{
    namespace Diagnostics = OSFSettings::API::Diagnostics;
    using OSFSettings::API::Status;

    constexpr auto kModId = "osfsettings-diagnostics-example";
    constexpr auto kFeatureIssue = "example-feature";

    void LogResult(const char* operation, Status status)
    {
        if (status == Status::Ok) {
            REX::INFO("{}: Ok", operation);
        } else {
            REX::ERROR("{}: status {}", operation, static_cast<std::uint32_t>(status));
        }
    }

    void RunExample()
    {
        Diagnostics::Client diagnostics;
        if (!diagnostics.Init()) {
            REX::WARN("Mod Issues API unavailable; skipping the development example");
            return;
        }
        REX::INFO("Mod Issues API acquired: {:#010x}", diagnostics.Version());

        // A mod ID is enough: this plugin supplies no settings schema.
        LogResult("Report warning", diagnostics.Report({
            .modId = kModId,
            .id = kFeatureIssue,
            .title = "Example feature is limited",
            .impact = "This simulates a feature running with reduced functionality.",
            .nextSteps = "Development example only; no player action is needed."
        }));

        // The same identity updates one entry. Omitted optional text is cleared.
        LogResult("Update to error", diagnostics.Report({
            .modId = kModId,
            .id = kFeatureIssue,
            .severity = Diagnostics::Severity::Error,
            .title = "Example feature is unavailable"
        }));

        LogResult("Report second issue", diagnostics.Report({
            .modId = kModId,
            .id = "example-resource",
            .title = "Example optional resource is unavailable"
        }));

        // Simulated recovery: clear one issue, then the remaining mod issues.
        LogResult("Clear feature issue", diagnostics.Clear(kModId, kFeatureIssue));
        LogResult("Clear remaining mod issues", diagnostics.ClearMod(kModId));

        // Leave illustrative reports active so the menu can be inspected manually.
        LogResult("Show sample error", diagnostics.Report({
            .modId = kModId,
            .id = "sample-error",
            .severity = Diagnostics::Severity::Error,
            .title = "Example: scene playback unavailable",
            .impact = "This is a simulated error. In a real report, this section explains which feature cannot run.",
            .nextSteps = "No repair is needed. Check that this error appears before the warnings and that both detail sections are readable."
        }));

        LogResult("Show sample warning", diagnostics.Report({
            .modId = kModId,
            .id = "sample-warning",
            .title = "Example: optional animation pack missing",
            .impact = "This is a simulated warning. Some optional scenes would be unavailable while other features continue to work.",
            .nextSteps = "No pack is actually missing. Check the warning styling and move between this report and the error."
        }));

        LogResult("Show sample title-only issue", diagnostics.Report({
            .modId = kModId,
            .id = "sample-title-only",
            .title = "Example: a report with no additional details"
        }));

        LogResult("Show sample long issue", diagnostics.Report({
            .modId = kModId,
            .id = "sample-long-text",
            .title = "Example: a longer report to check reading and scrolling",
            .impact = "This development sample exercises long issue text in both menu sizes. "
                "Its title and body should wrap without overlapping another section. The complete report "
                "should remain readable even when it is taller than the detail pane.\n\n"
                "The issue list should stay focused while you scroll the details. Mouse-wheel input over "
                "this pane should not move selection in the list. Keyboard Page Up and Page Down, and the "
                "displayed controller scroll controls, should reach all of the text.\n\n"
                "This report is intentionally verbose. It does not indicate a problem with your installed "
                "mods, save, or animation files. All four example reports belong to the development plugin "
                "and remain present until the game exits.",
            .nextSteps = "Scroll to the end of this report, then select a different issue. The new issue "
                "should start at the top. Return here and check that every paragraph can still be reached.\n\n"
                "Switch between All Mods and Mod Issues, then open an ordinary settings page and return. "
                "The sample reports should remain available. Try the large-text setting as well, where "
                "wrapping and the scroll distance will differ.\n\n"
                "To return to the empty state, disable OSFSettingsDiagnosticsExample.dll and restart "
                "Starfield. Only reports from other active providers should remain.\n\n"
                "END OF LONG REPORT"
        }));
    }

    void OnMessage(SFSE::MessagingInterface::Message* message)
    {
        if (message && message->type == SFSE::MessagingInterface::kPostPostLoad) RunExample();
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* sfse)
{
    SFSE::Init(sfse, { .logRotate = 1, .hook = false });
    const auto* messaging = SFSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(OnMessage)) {
        REX::ERROR("Could not register the development example's SFSE listener");
        return false;
    }
    return true;
}
