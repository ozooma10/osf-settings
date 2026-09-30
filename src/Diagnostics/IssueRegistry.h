#pragma once

#include "ModIssue.h"

#include <string_view>
#include <vector>

namespace OSFSettings
{
    // Issue ids and titles need a visible character and no embedded NUL.
    bool IsValidIssueText(std::string_view text);

    class IssueRegistry
    {
    public:
        bool Report(ModIssue issue);
        bool Clear(std::string_view modId, std::string_view id);

        std::size_t ClearMod(std::string_view modId);

        // Owned copy: errors before warnings, in original report order within each severity.
        std::vector<ModIssue> Snapshot() const;

    private:
        std::vector<ModIssue> m_issues;
    };
}
