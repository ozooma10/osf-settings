#pragma once

#include "IssueRegistry.h"

#include <mutex>

namespace OSFSettings
{
    class DiagnosticsService
    {
    public:
        static DiagnosticsService& Get();

        bool Report(ModIssue issue);
        bool Clear(std::string_view modId, std::string_view id);
        std::size_t ClearMod(std::string_view modId);
        std::vector<ModIssue> Snapshot() const;

    private:
        mutable std::mutex m_mutex;
        IssueRegistry m_registry;
    };
}
