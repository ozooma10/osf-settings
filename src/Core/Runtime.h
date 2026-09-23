#pragma once

#include "Settings/SettingsService.h"

namespace OSFSettings
{
    class Runtime
    {
    public:
        static Runtime& Get();
        bool Initialize() noexcept;
        std::vector<ModSettings> Settings() const { return SettingsService::Get().Snapshot(); }
        SettingsError SetValue(std::string_view mod, std::string_view key, SettingValue value);
        static void ReportLoadIssues(); // Repeatable; reports replace their earlier text in place.
    private:
        bool m_initialized{ false };
    };
}
