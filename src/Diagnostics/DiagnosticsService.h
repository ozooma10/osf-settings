#pragma once

#include "IssueRegistry.h"

#include <atomic>
#include <cstdint>
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
        std::uint64_t Revision() const noexcept { return m_revision.load(); }

    private:
        mutable std::mutex m_mutex;
        IssueRegistry m_registry;
        std::atomic_uint64_t m_revision{};
    };
}
