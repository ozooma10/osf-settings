#include "Issues.h"

namespace OSFSettings::Papyrus
{
    bool Issues::ReportIssue(const char* modId, const char* id, const char* title, bool isError, const char* impact, const char* nextSteps) const
    {
        return m_diagnostics.Report({
            .modId = modId,
            .id = id,
            .severity = isError ? API::Diagnostics::Severity::Error : API::Diagnostics::Severity::Warning,
            .title = title,
            .impact = impact,
            .nextSteps = nextSteps
        }) == API::Status::Ok;
    }

    bool Issues::ClearIssue(const char* modId, const char* id) const
    {
        return m_diagnostics.Clear(modId, id) == API::Status::Ok;
    }

    bool Issues::ClearModIssues(const char* modId) const
    {
        return m_diagnostics.ClearMod(modId) == API::Status::Ok;
    }
}
