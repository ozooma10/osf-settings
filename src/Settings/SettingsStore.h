#pragma once

#include "SettingsSchema.h"
#include "SettingsError.h"
#include "StateStore.h"

#include <filesystem>
#include <optional>
#include <memory>

namespace OSFSettings
{
    class SettingsStore
    {
    public:
        using Error = SettingsError;

        struct SetResult
        {
            bool ok{};
            std::string error;
            Error code{};
            bool changed{};
        };

        void LoadAll(const std::filesystem::path& schemaDir, const std::filesystem::path& valuesDir,
            std::shared_ptr<StateStore> state = {});
        std::optional<SettingValue> GetValue(std::string_view mod, std::string_view key) const;
        SetResult Set(std::string_view mod, std::string_view key, SettingValue value);
        SetResult Reset(std::string_view mod, std::string_view key);
        SetResult ResetMod(std::string_view mod);
        const ModSettings* FindMod(std::string_view mod) const;

        // Borrowed views for startup logging; valid until the next LoadAll.
        // Set replaces a values map, so do not retain references to its entries.
        const std::vector<ModSettings>& Mods() const { return m_mods; }
        const std::vector<SettingsLoadError>& LoadErrors() const { return m_loadErrors; }

    private:
        SetResult Commit(ModSettings& mod, SettingValues proposed);
        std::vector<ModSettings> m_mods;
        std::vector<SettingsLoadError> m_loadErrors;
        std::filesystem::path m_valuesDir;
        std::shared_ptr<StateStore> m_state;
    };
}
