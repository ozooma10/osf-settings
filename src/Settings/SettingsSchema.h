#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>

#include "SettingValue.h"

namespace OSFSettings
{
    struct BoolDefinition
    {
        bool defaultValue{};
    };

    struct IntDefinition
    {
        std::int64_t defaultValue{};
        std::optional<std::int64_t> minimum;
        std::optional<std::int64_t> maximum;
    };

    struct FloatDefinition
    {
        double defaultValue{};
        std::optional<double> minimum;
        std::optional<double> maximum;
    };

    struct SettingDefinition
    {
        std::string key;
        std::string label;
        std::string hint;
        std::variant<BoolDefinition, IntDefinition, FloatDefinition> definition;

        SettingValue DefaultValue() const;
    };

    bool IsValidValue(const SettingDefinition& setting, const SettingValue& value);

    struct SettingsGroup
    {
        std::string id;
        std::string label;
        std::vector<SettingDefinition> settings;
    };

    struct ModSchema
    {
        std::string id;
        std::string title;
        std::string description;
        std::vector<SettingsGroup> groups;
        const SettingDefinition* FindSetting(std::string_view key) const;
    };

    struct ModSettings
    {
        ModSchema schema;
        SettingValues values;
    };

    struct SettingsLoadError
    {
        std::filesystem::path file;
        std::string message;
    };  
}
