#pragma once

#include "Settings/SettingsService.h"

namespace OSFSettings::Runtime
{
    bool Initialize() noexcept; // Called once from Plugin::OnLoad.
    inline std::vector<ModSettings> Settings() { return SettingsService::Get().Snapshot(); }
    SettingsError SetValue(std::string_view mod, std::string_view key, SettingValue value);
    void ReportLoadIssues(); // Repeatable; reports replace their earlier text in place.
}
