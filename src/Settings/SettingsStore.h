#pragma once

#include "SettingsSchema.h"
#include "SettingsError.h"

#include <filesystem>
#include <optional>
#include <functional>

namespace OSFSettings
{
    class SettingsStore
    {
    public:
        using Error = SettingsError;

        struct SetResult
        {
            bool ok() const { return code == Error::None; }
            std::string error;
            Error code{};
            bool changed{};
        };

        void LoadAll(const std::filesystem::path& schemaDir, const std::filesystem::path& valuesDir);
        std::optional<SettingValue> GetValue(std::string_view mod, std::string_view key) const;
        SetResult Set(std::string_view mod, std::string_view key, SettingValue value);
        SetResult Reset(std::string_view mod, std::string_view key);
        SetResult ResetMod(std::string_view mod);
        const ModSettings* FindMod(std::string_view mod) const;
        ModSettings* FindMod(std::string_view mod);
        using Save = std::function<bool(const SettingValues&)>;
        Error RegisterProvider(ModSettings mod, Save save, std::uint64_t& registration);
        std::optional<std::string> UnregisterProvider(std::uint64_t registration);

        // Borrowed views for startup logging; valid until the next LoadAll.
        // Set replaces a values map, so do not retain references to its entries.
        const std::vector<ModSettings>& Mods() const { return m_mods; }
        const std::vector<SettingsLoadError>& LoadErrors() const { return m_loadErrors; }

    private:
        SetResult Commit(ModSettings& mod, SettingValues proposed);
        std::vector<ModSettings> m_mods;
        std::vector<SettingsLoadError> m_loadErrors;
        std::filesystem::path m_valuesDir;
        struct Provider { std::uint64_t token; Save save; };
        std::map<std::string, Provider, std::less<>> m_providers;
        std::uint64_t m_nextProvider{ 1 };
    };
}
