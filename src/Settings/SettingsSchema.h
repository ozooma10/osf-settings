#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <vector>

#include "SettingValue.h"

namespace OSFSettings
{
    enum class SettingType { Bool };

    struct SettingDefinition
    {
        std::string key;
        SettingType type{ SettingType::Bool };
        SettingValue defaultValue{ false };
        std::string label;
        std::string hint;
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
