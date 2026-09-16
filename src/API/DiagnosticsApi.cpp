#include "DiagnosticsApi.h"
#include "Diagnostics/DiagnosticsService.h"
#include "Settings/SettingsSchema.h"

namespace OSFSettings::API
{
    DiagnosticsApi& DiagnosticsApi::Get()
    {
        static auto* api = new DiagnosticsApi(DiagnosticsService::Get());
        return *api;
    }

    Status DiagnosticsApi::Report(const Diagnostics::Issue& issue) noexcept
    {
        if (!issue.modId || !issue.id || !issue.title) {
            return Status::InvalidArgument;
        }
        const auto accepted = m_service.Report({
            .modId = issue.modId,
            .id = issue.id,
            .severity = issue.severity == Diagnostics::Severity::Error ? IssueSeverity::Error : IssueSeverity::Warning,
            .title = issue.title,
            .impact = issue.impact ? issue.impact : "",
            .nextSteps = issue.nextSteps ? issue.nextSteps : ""
        });
        return accepted ? Status::Ok : Status::InvalidArgument;
    }

    Status DiagnosticsApi::Clear(const char* modId, const char* id) noexcept
    {
        if (!modId || !id || !IsValidModId(modId) || std::string_view(id).find_first_not_of(" \t\r\n\f\v") == std::string_view::npos) {
            return Status::InvalidArgument;
        }
        m_service.Clear(modId, id);
        return Status::Ok;
    }

    Status DiagnosticsApi::ClearMod(const char* modId) noexcept
    {
        if (!modId || !IsValidModId(modId)) return Status::InvalidArgument;
        m_service.ClearMod(modId);
        return Status::Ok;
    }
}
