#include "DiagnosticsService.h"

#include <utility>

namespace OSFSettings
{
    DiagnosticsService& DiagnosticsService::Get()
    {
        static auto* service = new DiagnosticsService;
        return *service;
    }

    bool DiagnosticsService::Report(ModIssue issue)
    {
        std::lock_guard lock(m_mutex);
        const bool accepted = m_registry.Report(std::move(issue));
        if (accepted) {
            m_revision++;
        }
        return accepted;
    }

    bool DiagnosticsService::Clear(std::string_view modId, std::string_view id)
    {
        std::lock_guard lock(m_mutex);
        const bool removed = m_registry.Clear(modId, id);
        if (removed) {
            m_revision++;
        }
        return removed;
    }

    std::size_t DiagnosticsService::ClearMod(std::string_view modId)
    {
        std::lock_guard lock(m_mutex);
        const auto removed = m_registry.ClearMod(modId);
        if (removed) {
            m_revision++;
        }
        return removed;
    }

    std::vector<ModIssue> DiagnosticsService::Snapshot() const
    {
        std::lock_guard lock(m_mutex);
        return m_registry.Snapshot();
    }
}
