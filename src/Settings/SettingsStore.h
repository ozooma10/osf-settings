#pragma once

#include "SettingsSchema.h"

#include <filesystem>
#include <optional>

namespace OSFSettings
{
    class SettingsStore
    {
    public:
        struct SetResult
        {
            bool ok{};
            std::string error;
        };

        void LoadAll(const std::filesystem::path& schemaDir, const std::filesystem::path& valuesDir);
        std::optional<SettingValue> GetValue(std::string_view mod, std::string_view key) const;
        SetResult Set(std::string_view mod, std::string_view key, SettingValue value);

        // Borrowed views for startup logging; valid until the next LoadAll.
        // Set replaces a values map, so do not retain references to its entries.
        const std::vector<ModSettings>& Mods() const { return m_mods; }
        const std::vector<SettingsLoadError>& LoadErrors() const { return m_loadErrors; }

    private:
        std::vector<ModSettings> m_mods;
        std::vector<SettingsLoadError> m_loadErrors;
        std::filesystem::path m_valuesDir;
    };
}
