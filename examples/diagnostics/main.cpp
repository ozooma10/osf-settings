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
