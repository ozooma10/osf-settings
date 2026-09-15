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
        double step{ 0.1 }; // Editor increment; stored values need not lie on this grid.
    };

    struct EnumOption
    {
        std::string value;
        std::string label;
    };

    struct EnumDefinition
    {
        std::string defaultValue;
        std::vector<EnumOption> options;
    };

    struct KeyDefinition
    {
        KeyBinding defaultValue;
        bool allowUnbound{};
    };

    struct SettingDefinition
    {
        std::string key;
        std::string label;
        std::string hint;
        std::variant<BoolDefinition, IntDefinition, FloatDefinition, EnumDefinition, KeyDefinition> definition;

        SettingValue DefaultValue() const;
    };

    bool IsValidModId(std::string_view id);
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
