#include "SettingsStore.h"

namespace OSFSettings
{
    void SettingsStore::LoadAll(const std::filesystem::path &a_schemaDir, const std::filesystem::path &a_valuesDir)
    {
        m_mods.clear();
        m_loadErrors.clear();
        m_valuesDir = a_valuesDir;

        std::error_code ec;
        if (!std::filesystem::is_directory(a_schemaDir, ec)) return;
        std::vector<std::filesystem::path> files;
        for (std::filesystem::directory_iterator it(a_schemaDir, std::filesystem::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            std::error_code entryEc;
            if (it->is_regular_file(entryEc) && it->path().extension() == ".json") {
                files.push_back(it->path());
            }
        }
        std::ranges::sort(files);
        for (const auto& path : files) {
            const auto document = Json::ParseFile(path);
            auto schema = document ? SettingsJson::ParseSchema(*document, true) : std::nullopt;
        }
    }

    std::optional<SettingValue> SettingsStore::GetValue(std::string_view a_mod, std::string_view a_key) const
    {
        return std::optional<SettingValue>();
    }

    const ModSchema *SettingsStore::GetSchema(std::string_view a_mod) const
    {
        return nullptr;
    }

    const SettingDefinition *SettingsStore::GetSetting(std::string_view a_mod, std::string_view a_key) const
    {
        return nullptr;
    }

    std::optional<SettingType> SettingsStore::GetSettingType(std::string_view a_mod, std::string_view a_key) const
    {
        return std::optional<SettingType>();
    }

    bool SettingsStore::Set(std::string_view a_mod, std::string_view a_key, const SettingValue &a_value)
    {
        return false;
    }

    void SettingsStore::AddSchema(ModSchema schema)
    {
    }

    StoredMod* SettingsStore::FindMod(std::string_view id)
    {
        return nullptr;
    }

    const StoredMod *SettingsStore::FindMod(std::string_view id) const
    {
        return nullptr;
    }
}