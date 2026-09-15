#pragma once

#include "SettingsSchema.h"

#include <optional>
#include <nlohmann/json_fwd.hpp>

namespace OSFSettings::SettingsJson
{
    std::optional<std::int64_t> DecodeInteger(const nlohmann::json& value);
    std::optional<double> DecodeFloat(const nlohmann::json& value);
    std::optional<SettingValue> DecodeValue(const nlohmann::json& value, const SettingDefinition& setting);
    std::optional<ModSchema> ParseSchema(const nlohmann::json& document, std::string& error);
    void LoadValues(const std::filesystem::path& path, const ModSchema& schema, SettingValues& values, std::vector<SettingsLoadError>& errors);
    bool SaveValues(const std::filesystem::path& path, const SettingValues& values, std::string& error);
}
