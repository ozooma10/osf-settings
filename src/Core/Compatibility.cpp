#include "Compatibility.h"
#include "Diagnostics/DiagnosticsService.h"
#include "Settings/SettingsService.h"
#include "SFSE/API.h"

#include <mutex>

namespace OSFSettings
{
    void ReportCompatibilityIssues()
    {
        // Serialize startup/localization and dispatcher reports so an older snapshot cannot win.
        static std::mutex mutex;
        static std::optional<std::uint64_t> reportedRevision;
        std::lock_guard lock(mutex);
        auto& settings = SettingsService::Get();
        const auto revision = settings.SchemaRevision();
        if (reportedRevision == revision) return;
        auto& diagnostics = DiagnosticsService::Get();
        auto mods = settings.Snapshot();
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
        // Read before Snapshot: a concurrent provider change must still trigger the next pass.
        reportedRevision = revision;
    }
}
