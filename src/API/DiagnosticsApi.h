#pragma once

#include "../../sdk/OSFSettings_Diagnostics.h"

namespace OSFSettings { class DiagnosticsService; }

namespace OSFSettings::API
{
    class DiagnosticsApi final : public Diagnostics::IDiagnostics
    {
    public:
        static DiagnosticsApi& Get();
        explicit DiagnosticsApi(DiagnosticsService& service) : m_service(service) {}

        Status Report(const Diagnostics::Issue& issue) noexcept override;
        Status Clear(const char* modId, const char* id) noexcept override;
        Status ClearMod(const char* modId) noexcept override;

    private:
        DiagnosticsService& m_service;
    };
}
