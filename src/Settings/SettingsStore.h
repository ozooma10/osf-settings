#pragma once

#include "SettingsSchema.h"

#include <filesystem>
#include <optional>

namespace OSFSettings
{
    class SettingsStore
    {
    public:
        void LoadAll(const std::filesystem::path& schemaDir);
        std::optional<SettingValue> GetValue(std::string_view mod, std::string_view key) const;

        // Borrowed views for startup logging; valid until the next LoadAll.
        const std::vector<ModSettings>& Mods() const { return m_mods; }
        const std::vector<SettingsLoadError>& LoadErrors() const { return m_loadErrors; }

    private:
        std::vector<ModSettings> m_mods;
        std::vector<SettingsLoadError> m_loadErrors;
    };
}
