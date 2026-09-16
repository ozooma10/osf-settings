#include "IssueRegistry.h"
#include "Settings/SettingsSchema.h"

#include <algorithm>
#include <utility>

namespace OSFSettings
{
    namespace
    {
        bool ValidText(std::string_view text)
        {
            return text.find('\0') == std::string_view::npos && text.find_first_not_of(" \t\r\n\f\v") != std::string_view::npos;
        }
    }

    bool IssueRegistry::Report(ModIssue issue)
    {
        if (!IsValidModId(issue.modId) || !ValidText(issue.id) || !ValidText(issue.title) || !ValidText(issue.impact) || !ValidText(issue.nextSteps)) {
            return false;
        }
        
        const auto existing = std::ranges::find_if(m_issues, [&](const ModIssue& current) {
            return current.modId == issue.modId && current.id == issue.id;
        });
        if (existing != m_issues.end()) {
            *existing = std::move(issue);
        } else {
            m_issues.push_back(std::move(issue));
        }
        return true;
    }

    bool IssueRegistry::Clear(std::string_view modId, std::string_view id)
    {
        return std::erase_if(m_issues, [&](const ModIssue& issue) {
            return issue.modId == modId && issue.id == id;
        }) != 0;
    }

    std::size_t IssueRegistry::ClearMod(std::string_view modId)
    {
        return std::erase_if(m_issues, [&](const ModIssue& issue) { return issue.modId == modId; });
    }

    std::vector<ModIssue> IssueRegistry::Snapshot() const
    {
        auto issues = m_issues;
        std::stable_partition(issues.begin(), issues.end(), [](const ModIssue& issue) {
            return issue.severity == IssueSeverity::Error;
        });
        return issues;
    }
}
