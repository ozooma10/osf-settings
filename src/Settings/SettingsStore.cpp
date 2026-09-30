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
            std::optional<SettingsVersion> expectedSettingsVersion;
            auto schema = SettingsJson::ParseSchema(input, path.stem().string(), message, &expectedSettingsVersion);
            if (!schema) {
                m_loadErrors.push_back({ path, std::move(message), true, expectedSettingsVersion });
                continue;
            }

            ModSettings mod;
            mod.schema = std::move(*schema);
            mod.values = SettingsJson::LoadValues(m_valuesDir / (mod.schema.id + ".json"), mod.schema, m_loadErrors);
            m_mods.push_back(std::move(mod));

        }
    }

    std::optional<SettingValue> SettingsStore::GetValue(std::string_view mod, std::string_view key) const
    {
        const auto* stored = FindMod(mod);
        if (!stored) return std::nullopt;
        return stored->GetValue(key);
    }

    const ModSettings* SettingsStore::FindMod(std::string_view mod) const
    {
        const auto found = std::ranges::find(m_mods, mod, [](const auto& stored) { return std::string_view(stored.schema.id); });
        return found != m_mods.end() ? &*found : nullptr;
    }

    ModSettings* SettingsStore::FindMod(std::string_view mod)
    {
        return const_cast<ModSettings*>(static_cast<const SettingsStore&>(*this).FindMod(mod));
    }

    SettingsStore::SetResult SettingsStore::Commit(ModSettings& mod, SettingValues proposed, bool changed)
    {
        if (mod.values == proposed) return {};
        const auto provider = m_providers.find(mod.schema.id);
        std::string error;
        const bool saved = provider != m_providers.end() ? provider->second.save(proposed) :
            SettingsJson::SaveValues(m_valuesDir / (mod.schema.id + ".json"), proposed, error);
        if (!saved) {
            if (error.empty()) error = "settings provider could not save " + mod.schema.id;
            return { std::move(error), Error::SaveFailed };
        }
        mod.values.swap(proposed);
        return { {}, Error::None, changed };
    }

    SettingsStore::Error SettingsStore::RegisterProvider(ModSettings mod, Save save, std::uint64_t& registration)
    {
        auto* existing = FindMod(mod.schema.id);
        const auto owner = m_providers.find(mod.schema.id);
        if (existing) {
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
        auto* stored = FindMod(mod);
        if (!stored) return { "unknown mod id", Error::UnknownMod };
        const auto* setting = stored->schema.FindSetting(key);
        if (!setting) {
            return { "unknown setting key", Error::UnknownSetting };
        }
        const auto current = stored->GetValue(key);
        if (current->index() != value.index()) {
            return { "value does not match the setting's type", Error::TypeMismatch };
        }
        if (!IsValidValue(*setting, value)) {
            return { "value does not match the setting's type or validation rules", Error::InvalidValue };
        }
        if (*current == value) return {};

        // Publish an explicit edit only after saving the sparse values succeeds.
        auto proposed = stored->values;
        proposed.insert_or_assign(std::string(key), std::move(value));
        return Commit(*stored, std::move(proposed), true);
    }

    SettingsStore::SetResult SettingsStore::Reset(std::string_view mod, std::string_view key)
    {
        auto* stored = FindMod(mod);
        if (!stored) return { "unknown mod id", Error::UnknownMod };
        const auto* setting = stored->schema.FindSetting(key);
        if (!setting) return { "unknown setting key", Error::UnknownSetting };
        const auto defaultValue = setting->DefaultValue();
        if (m_providers.contains(stored->schema.id)) return Set(mod, key, defaultValue);

        auto proposed = stored->values;
        proposed.erase(std::string(key));
        return Commit(*stored, std::move(proposed), stored->GetValue(key) != defaultValue);
    }

    SettingsStore::SetResult SettingsStore::ResetMod(std::string_view mod)
    {
        auto* stored = FindMod(mod);
        if (!stored) return { "unknown mod id", Error::UnknownMod };
        auto defaults = stored->schema.DefaultValues();
        const bool changed = std::ranges::any_of(stored->values, [&](const auto& saved) {
            return saved.second != defaults.at(saved.first);
        });
        return Commit(*stored, m_providers.contains(stored->schema.id) ? std::move(defaults) : SettingValues{}, changed);
    }
}
