#pragma once

#include <string>
#include <string_view>
#include <optional>

#include "SettingValue.h"

namespace OSFSettings
{
    enum class SettingType { Bool, Int, Float, Enum, String, Key, Action, Note };
    enum class SettingRequirement { None, Restart, Reload };

    struct SettingDefinition
    {
        std::string key;
        SettingType type { SettingType::Bool };
        std::optional<std::string> defaultValue;

        //Display Text
        std::optional<std::string> label;
        std::optional<std::string> hint;
        std::optional<std::string> text;

        SettingRequirement requirement;
    };

    struct SettingsGroup
    {
        std::string id;
        std::string label;
        std::vector<SettingDefinition> settings;
    };

    struct ModSchema
    {
        std::string id;
        std::optional<std::string> title;
        std::optional<std::string> description;
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
        std::string kind;
        std::string file;
        std::string mod;
        std::string message;
    };  
}