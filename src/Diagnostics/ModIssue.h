#pragma once

#include <span>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace OSFSettings
{
    struct ModSettings;
    struct SettingsLoadError;

    enum class IssueSeverity
    {
        Warning,
        Error
    };

    struct ModIssue
    {
        std::string modId;
        std::string id; // Stable within the reporting mod.
        IssueSeverity severity{ IssueSeverity::Warning };
        std::string title;
        std::string impact;
        std::string nextSteps;
        std::uint32_t nexusModId{}; // Optional Starfield Nexus page; zero means no link.
        std::string reason; // Exact cause for built-in failures.
        std::string note; // Reference text for built-in reports.
        // Display names of the mods a built-in report affects; empty means the reporting mod.
        std::vector<std::string> affectedMods;
        // Version gaps show as "installed -> required"; an empty installed version means not installed.
        std::string installedVersion;
        std::string requiredVersion;
    };

    std::string IssueModName(const ModIssue& issue, std::span<const ModSettings> settings);
    // Plain text a player can paste into a bug report; empty fields are omitted.
    std::string IssueReport(const ModIssue& issue, std::span<const ModSettings> settings, std::string_view pluginVersion);

    // OSF owns these reports so a mod's own ClearMod cannot hide a broken schema.
    std::vector<ModIssue> SchemaLoadIssues(std::span<const SettingsLoadError> errors);
    // One warning per declared dependency whose manifest is missing or older than a consumer asks for.
    std::vector<ModIssue> DependencyIssues(std::span<const ModSettings> settings, std::span<const SettingsLoadError> errors = {});
    // One warning per mod whose extends names an installed mod but still could not join its page.
    std::vector<ModIssue> ExtensionIssues(std::span<const ModSettings> settings);
}
