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
        return m_registry.Report(std::move(issue));
    }

    bool DiagnosticsService::Clear(std::string_view modId, std::string_view id)
    {
        std::lock_guard lock(m_mutex);
        return m_registry.Clear(modId, id);
    }

    std::size_t DiagnosticsService::ClearMod(std::string_view modId)
    {
        std::lock_guard lock(m_mutex);
        return m_registry.ClearMod(modId);
    }

    std::vector<ModIssue> DiagnosticsService::Snapshot() const
    {
        std::lock_guard lock(m_mutex);
        return m_registry.Snapshot();
    }
}
