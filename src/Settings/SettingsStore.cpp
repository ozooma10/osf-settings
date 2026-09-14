#include "SettingsStore.h"
#include "SettingsJson.h"

#include <algorithm>
#include <fstream>
#include <utility>
#include <nlohmann/json.hpp>

namespace OSFSettings
{
    void SettingsStore::LoadAll(const std::filesystem::path& schemaDir)
    {
        m_mods.clear();
        m_loadErrors.clear();

        std::error_code error;
        if (!std::filesystem::is_directory(schemaDir, error)) {
            m_loadErrors.push_back({ schemaDir, error ? error.message() : "schema directory is missing or is not a directory" });
            return;
        }

        std::vector<std::filesystem::path> files;
        for (std::filesystem::directory_iterator it(schemaDir, error), end; it != end && !error; it.increment(error)) {
            std::error_code entryError;
            if (it->is_regular_file(entryError) && it->path().extension() == ".json") {
                files.push_back(it->path());
            }
            if (entryError) m_loadErrors.push_back({ it->path(), entryError.message() });
        }
        if (error) m_loadErrors.push_back({ schemaDir, error.message() });
        std::ranges::sort(files);

        for (const auto& path : files) {
            try {
                std::ifstream input(path);
                if (!input) {
                    m_loadErrors.push_back({ path, "cannot open schema file" });
                    continue;
                }
                const auto document = nlohmann::json::parse(input);
                std::string message;
                auto schema = SettingsJson::ParseSchema(document, message);
                if (!schema) {
                    m_loadErrors.push_back({ path, std::move(message) });
                    continue;
                }
                if (schema->id != path.stem().string()) {
                    m_loadErrors.push_back({ path, "schema id must match the filename stem" });
                    continue;
                }

                ModSettings mod;
                mod.schema = std::move(*schema);
                for (const auto& group : mod.schema.groups) {
                    for (const auto& setting : group.settings) {
                        mod.values.emplace(setting.key, setting.defaultValue);
                    }
                }
                m_mods.push_back(std::move(mod));
            } catch (const std::exception& exception) {
                m_loadErrors.push_back({ path, exception.what() });
            }
        }
    }

    std::optional<SettingValue> SettingsStore::GetValue(std::string_view mod, std::string_view key) const
    {
        for (const auto& stored : m_mods) {
            if (stored.schema.id != mod) continue;
            const auto value = stored.values.find(key);
            if (value != stored.values.end()) return value->second;
            return std::nullopt;
        }
        return std::nullopt;
    }
}
