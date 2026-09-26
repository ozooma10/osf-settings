#include "SettingsStore.h"
#include "SettingsJson.h"

#include <algorithm>
#include <fstream>
#include <utility>

namespace OSFSettings
{
    void SettingsStore::LoadAll(const std::filesystem::path& schemaDir, const std::filesystem::path& valuesDir)
    {
        m_mods.clear();
        m_providers.clear();
        m_loadErrors.clear();
        m_valuesDir = valuesDir;

        std::error_code error;
        if (!std::filesystem::is_directory(schemaDir, error)) {
            m_loadErrors.push_back({ schemaDir, error ? error.message() : "schema directory is missing or is not a directory", true });
            return;
        }

        std::vector<std::filesystem::path> files;
        for (std::filesystem::directory_iterator it(schemaDir, error), end; it != end && !error; it.increment(error)) {
            std::error_code entryError;
            if (it->is_regular_file(entryError) && it->path().extension() == ".json") {
                files.push_back(it->path());
            }
            if (entryError) m_loadErrors.push_back({ it->path(), entryError.message(), true });
        }
        if (error) m_loadErrors.push_back({ schemaDir, error.message(), true });
        std::ranges::sort(files);

        for (const auto& path : files) {
            std::ifstream input(path);
            if (!input) {
                m_loadErrors.push_back({ path, "cannot open schema file", true });
                continue;
            }
            std::string message;
            auto schema = SettingsJson::ParseSchema(input, path.stem().string(), message);
            if (!schema) {
                m_loadErrors.push_back({ path, std::move(message), true });
                continue;
            }

            ModSettings mod;
            mod.schema = std::move(*schema);
            for (const auto& group : mod.schema.groups) {
                for (const auto& control : group.controls) {
                    const auto* valueSetting = std::get_if<SettingDefinition>(&control);
                    if (!valueSetting) continue;
                    const auto& setting = *valueSetting;
                    mod.values.emplace(setting.key, setting.DefaultValue());
                }
            }
            SettingsJson::LoadValues(m_valuesDir / (mod.schema.id + ".json"), mod.schema, mod.values, m_loadErrors);
            m_mods.push_back(std::move(mod));

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

    const ModSettings* SettingsStore::FindMod(std::string_view mod) const
    {
        const auto found = std::ranges::find(m_mods, mod, [](const auto& stored) { return std::string_view(stored.schema.id); });
        return found != m_mods.end() ? &*found : nullptr;
    }

    SettingsStore::SetResult SettingsStore::Commit(ModSettings& mod, SettingValues proposed)
    {
        if (mod.values == proposed) return { true, {} };
        std::string error;
        const auto provider = m_providers.find(mod.schema.id);
        const bool saved = provider != m_providers.end() ? provider->second.save(proposed) :
            SettingsJson::SaveValues(m_valuesDir / (mod.schema.id + ".json"), proposed, error);
        if (!saved) {
            if (error.empty()) error = "settings provider could not save " + mod.schema.id;
            return { false, std::move(error), Error::SaveFailed };
        }
        mod.values.swap(proposed);
        return { true, {}, Error::None, true };
    }

    SettingsStore::Error SettingsStore::RegisterProvider(ModSettings mod, Save save, std::uint64_t& registration)
    {
        const auto existing = std::ranges::find(m_mods, mod.schema.id, [](const auto& entry) { return entry.schema.id; });
        const auto owner = m_providers.find(mod.schema.id);
        if (existing != m_mods.end()) {
            if (owner == m_providers.end() || !registration || owner->second.token != registration) return Error::AlreadyRegistered;
            for (auto& [key, value] : mod.values) {
                const auto old = existing->values.find(key);
                const auto* setting = mod.schema.FindSetting(key);
                if (old != existing->values.end() && setting && IsValidValue(*setting, old->second)) value = old->second;
            }
            *existing = std::move(mod);
            owner->second.save = std::move(save);
        } else {
            if (registration || !m_nextProvider) return Error::InvalidArgument;
            registration = m_nextProvider++;
            m_providers.emplace(mod.schema.id, Provider{ registration, std::move(save) });
            m_mods.push_back(std::move(mod));
        }
        return Error::None;
    }

    std::optional<std::string> SettingsStore::UnregisterProvider(std::uint64_t registration)
    {
        const auto found = std::ranges::find_if(m_providers, [registration](const auto& entry) { return entry.second.token == registration; });
        if (found == m_providers.end()) return std::nullopt;
        const auto id = found->first;
        std::erase_if(m_mods, [&](const auto& mod) { return mod.schema.id == id; });
        m_providers.erase(found);
        return id;
    }

    SettingsStore::SetResult SettingsStore::Set(std::string_view mod, std::string_view key, SettingValue value)
    {
        for (auto& stored : m_mods) {
            if (stored.schema.id != mod) continue;
            const auto current = stored.values.find(key);
            const auto* setting = stored.schema.FindSetting(key);
            if (current == stored.values.end() || !setting) {
                return { false, "unknown setting key", Error::UnknownSetting };
            }
            if (current->second.index() != value.index()) {
                return { false, "value does not match the setting's type", Error::TypeMismatch };
            }
            if (!IsValidValue(*setting, value)) {
                return { false, "value does not match the setting's type or validation rules", Error::InvalidValue };
            }
            if (current->second == value) {
                return { true, {} };
            }

            // Propose the edit in a copy. The live value changes only after saving.
            auto proposed = stored.values;
            proposed.find(key)->second = value;
            return Commit(stored, std::move(proposed));
        }
        return { false, "unknown mod id", Error::UnknownMod };
    }

    SettingsStore::SetResult SettingsStore::Reset(std::string_view mod, std::string_view key)
    {
        const auto* stored = FindMod(mod);
        if (!stored) return { false, "unknown mod id", Error::UnknownMod };
        const auto* setting = stored->schema.FindSetting(key);
        if (!setting) return { false, "unknown setting key", Error::UnknownSetting };
        return Set(mod, key, setting->DefaultValue());
    }

    SettingsStore::SetResult SettingsStore::ResetMod(std::string_view mod)
    {
        for (auto& stored : m_mods) {
            if (stored.schema.id != mod) continue;
            SettingValues proposed;
            for (const auto& group : stored.schema.groups) {
                for (const auto& control : group.controls) {
                    const auto* valueSetting = std::get_if<SettingDefinition>(&control);
                    if (!valueSetting) continue;
                    const auto& setting = *valueSetting;
                    proposed.emplace(setting.key, setting.DefaultValue());
                }
            }
            return Commit(stored, std::move(proposed));
        }
        return { false, "unknown mod id", Error::UnknownMod };
    }
}
