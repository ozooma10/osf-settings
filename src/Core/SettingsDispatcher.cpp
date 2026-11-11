#include "SettingsDispatcher.h"
#include "Compatibility.h"
#include "Settings/SettingsService.h"
#include "Harness/TestHarness.h"
#include "SFSE/SFSE.h"

namespace OSFSettings::SettingsDispatcher
{
    bool Install()
    {
        const auto* tasks = SFSE::GetTaskInterface();
        if (!tasks) return false;
        static bool installed{};
        if (!installed) {
            tasks->AddPermanentTask([] {
                TestHarness::Poll();
                auto& settings = SettingsService::Get();
                if (settings.HasPendingChanges()) {
                    settings.DispatchChanges();
                }
                ReportCompatibilityIssues();
            });
            installed = true;
            REX::INFO("Settings notifications: SFSE dispatcher installed");
        }
        return true;
    }
}
