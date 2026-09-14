#pragma once

#include "SettingsSchema.h"

#include <filesystem>

namespace OSFSettings
{
    class SettingsStore
    {
    public:
        void LoadAll(const std::filesystem::path& a_schemaDir, const std::filesystem::path& a_valuesDir);
        
        std::optional<SettingValue> GetValue(std::string_view a_mod, std::string_view a_key) const;
        const ModSchema* GetSchema(std::string_view a_mod) const;
        const SettingDefinition* GetSetting(std::string_view a_mod, std::string_view a_key) const;
        std::optional<SettingType> GetSettingType(std::string_view  a_mod, std::string_view a_key) const;

        bool Set(std::string_view a_mod, std::string_view a_key, const SettingValue& a_value);
    private:
        struct StoredMod
        {
            ModSchema schema;
            SettingValues values;
            std::filesystem::path valuesPath;
            bool dirty{};
        };

        void AddSchema(ModSchema schema);
        StoredMod* FindMod(std::string_view id);
        const StoredMod* FindMod(std::string_view id) const;

        std::vector<StoredMod> m_mods;
        std::vector<SettingsLoadError> m_loadErrors;
        std::filesystem::path m_valuesDir;
    };
}