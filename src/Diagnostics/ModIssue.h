#pragma once

#include <span>
#include <string>

namespace OSFSettings
{
    struct ModSettings;

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
}
