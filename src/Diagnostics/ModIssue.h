#pragma once

#include <span>
#include <string>
#include <vector>

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
    };

    std::string IssueModName(const ModIssue& issue, std::span<const ModSettings> settings);

    // OSF owns these reports so a mod's own ClearMod cannot hide a broken schema.
    std::vector<ModIssue> SchemaLoadIssues(std::span<const SettingsLoadError> errors);
}
