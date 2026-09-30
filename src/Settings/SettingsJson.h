#pragma once

#include "SettingsSchema.h"

#include <iosfwd>
#include <optional>
#include <string_view>
#include <nlohmann/json_fwd.hpp>

namespace OSFSettings::SettingsJson
{
    std::optional<std::int64_t> DecodeInteger(const nlohmann::json& value);
    std::optional<double> DecodeFloat(const nlohmann::json& value);
    std::optional<SettingValue> DecodeValue(const nlohmann::json& value, const SettingDefinition& setting);
    // modId is the source filename stem; an optional legacy root id must match it.
    std::optional<ModSchema> ParseSchema(const nlohmann::ordered_json& document, std::string_view modId, std::string& error);
    // Parse source directly to preserve group order and reject duplicate group names.
    std::optional<ModSchema> ParseSchema(std::istream& input, std::string_view modId, std::string& error);
    // Keys store their key code and enums their value. Throws when a float is not finite.
    nlohmann::json EncodeValues(const SettingValues& values);
    void LoadValues(const std::filesystem::path& path, const ModSchema& schema, SettingValues& values, std::vector<SettingsLoadError>& errors);
    bool SaveValues(const std::filesystem::path& path, const SettingValues& values, std::string& error);
}
