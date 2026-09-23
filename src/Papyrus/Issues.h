#pragma once

#include "API/DiagnosticsApi.h"

namespace OSFSettings::Papyrus
{
    class Issues
    {
    public:
        explicit Issues(API::Diagnostics::IDiagnostics& diagnostics) : m_diagnostics(diagnostics) {}

        bool ReportIssue(const char* modId, const char* id, const char* title, bool isError, const char* impact, const char* nextSteps) const;
        bool ClearIssue(const char* modId, const char* id) const;
        bool ClearModIssues(const char* modId) const;

    private:
        API::Diagnostics::IDiagnostics& m_diagnostics;
    };
}
