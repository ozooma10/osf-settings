#pragma once

#include "SettingsSchema.h"

#include <optional>
#include <nlohmann/json_fwd.hpp>

namespace OSFSettings::SettingsJson
{
    std::optional<ModSchema> ParseSchema(const nlohmann::json& document, std::string& error);
    void LoadValues(const std::filesystem::path& path, SettingValues& values, std::vector<SettingsLoadError>& errors);
    bool SaveValues(const std::filesystem::path& path, const SettingValues& values, std::string& error);
}
