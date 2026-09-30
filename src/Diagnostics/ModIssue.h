#pragma once

#include <span>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace OSFSettings
{
    struct ModSettings;
    struct SettingsLoadError;
    struct SettingsVersion;

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
        std::string reason; // Exact cause for built-in settings-file failures.
    };

    std::string IssueModName(const ModIssue& issue, std::span<const ModSettings> settings);

    // OSF owns these reports so a mod's own ClearMod cannot hide a broken schema.
    std::vector<ModIssue> SchemaLoadIssues(std::span<const SettingsLoadError> errors);
    std::optional<ModIssue> SettingsUpdateIssue(std::span<const ModSettings> settings,
        std::span<const SettingsLoadError> errors, const SettingsVersion& installed);
}
