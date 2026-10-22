#include "Compatibility.h"
#include "Diagnostics/DiagnosticsService.h"
#include "Settings/SettingsService.h"

namespace OSFSettings
{
    void ReportCompatibilityIssues()
    {
        auto& diagnostics = DiagnosticsService::Get();
        auto mods = SettingsService::Get().Snapshot();
        // The running plugin, not its manifest, is the installed OSF Settings version.
        const auto installed = SFSE::GetPluginVersion();
        for (auto& mod : mods) {
            if (mod.schema.id == "osfsettings") {
                mod.schema.version = SettingsVersion{ { installed[0], installed[1], installed[2] } };
            }
        }

        // Clear only resolved dependency reports so unchanged ones keep their place.
        auto dependencies = DependencyIssues(mods);
        for (const auto& old : diagnostics.Snapshot()) {
            if (old.modId == "osfsettings" && old.id.starts_with("dependency:") &&
                std::ranges::none_of(dependencies, [&](const ModIssue& issue) { return issue.id == old.id; })) {
                diagnostics.Clear(old.modId, old.id);
            }
        }
        for (auto& issue : dependencies) {
            diagnostics.Report(std::move(issue));
        }
    }
}
