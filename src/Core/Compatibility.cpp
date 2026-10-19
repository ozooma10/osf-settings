#include "Compatibility.h"
#include "Diagnostics/DiagnosticsService.h"
#include "Settings/Localization.h"
#include "Settings/SettingsService.h"

#include <Windows.h>

namespace OSFSettings
{
    namespace
    {
        std::optional<REL::Version> InstalledOSFUIVersion()
        {
            const std::filesystem::path game{ REX::FModule::GetExecutingModule().GetFileName() };
            const auto dll = game.parent_path() / "Data" / "SFSE" / "Plugins" / "OSFUI.dll";
            const auto size = GetFileVersionInfoSizeW(dll.c_str(), nullptr);
            if (!size) return std::nullopt;
            std::vector<std::byte> data(size);
            if (!GetFileVersionInfoW(dll.c_str(), 0, size, data.data())) return std::nullopt;
            void* value{};
            UINT length{};
            if (!VerQueryValueW(data.data(), L"\\", &value, &length) || length < sizeof(VS_FIXEDFILEINFO)) return std::nullopt;
            const auto& info = *static_cast<const VS_FIXEDFILEINFO*>(value);
            if (info.dwSignature != 0xFEEF04BD) return std::nullopt;
            return REL::Version{ HIWORD(info.dwFileVersionMS), LOWORD(info.dwFileVersionMS),
                HIWORD(info.dwFileVersionLS), LOWORD(info.dwFileVersionLS) };
        }
    }

    void ReportCompatibilityIssues()
    {
        auto& diagnostics = DiagnosticsService::Get();
        auto mods = SettingsService::Get().Snapshot();
        // The running plugin, not its manifest, is the installed OSF Settings version.
        const auto installed = SFSE::GetPluginVersion();
        for (auto& mod : mods) {
            if (mod.schema.id == "osfsettings") mod.schema.version = SettingsVersion{ { installed[0], installed[1], installed[2] } };
        }

        // Clear only resolved dependency reports so unchanged ones keep their place.
        auto dependencies = DependencyIssues(mods);
        for (const auto& old : diagnostics.Snapshot()) {
            if (old.modId == "osfsettings" && old.id.starts_with("dependency:") &&
                std::ranges::none_of(dependencies, [&](const ModIssue& issue) { return issue.id == old.id; })) {
                diagnostics.Clear(old.modId, old.id);
            }
        }
        for (auto& issue : dependencies) diagnostics.Report(std::move(issue));

        // Read the active game's virtual Data path once, without loading the plugin.
        static const auto version = InstalledOSFUIVersion();
        if (!version || *version >= REL::Version{ 2, 0, 0, 0 }) return;
        DiagnosticsService::Get().Report({
            .modId = "osfsettings",
            .id = "osfui-update-required",
            .severity = IssueSeverity::Warning,
            .title = tr("issues.osfuiUpdate"),
            .impact = tr("issues.osfuiUpdateImpact", {{ "version", version->string() }}),
            .nextSteps = tr("issues.osfuiUpdateNextSteps"),
            .nexusModId = 17711
        });
    }
}
