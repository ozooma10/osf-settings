#include "Localization.h"
#include "English.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <optional>
#include <set>
#include <nlohmann/json.hpp>

namespace OSFSettings::Localization
{
    namespace
    {
        using Json = nlohmann::json;

        std::optional<std::set<std::string>> Placeholders(std::string_view text)
        {
            std::set<std::string> result;
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (text[i] == '}') return std::nullopt;
                if (text[i] != '{') continue;
                const auto end = text.find('}', i + 1);
                if (end == text.npos || end == i + 1) return std::nullopt;
                const auto name = text.substr(i + 1, end - i - 1);
                if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != name.npos) return std::nullopt;
                result.emplace(name);
                i = end;
            }
            return result;
        }

        bool ValidText(const Json& value, bool multiline, bool empty)
        {
            if (!value.is_string()) return false;
            const auto& text = value.get_ref<const std::string&>();
            if (!empty && text.empty()) return false;
            return std::ranges::none_of(text, [multiline](unsigned char c) {
                return (c < 32 && !(multiline && (c == '\n' || c == '\r' || c == '\t'))) || c == 127;
            });
        }

        struct Overlay
        {
            std::filesystem::path file;
            std::vector<SettingsLoadError>& errors;

            void Error(const std::string& field) { errors.push_back({ file, "invalid or unknown translation field: " + field }); }

            void Fields(const Json& object, std::initializer_list<std::string_view> names, const std::string& path)
            {
                for (const auto& [key, value] : object.items()) {
                    if (std::ranges::find(names, key) == names.end()) {
                        Error(path + key);
                    }
                }
            }

            void Text(const Json& object, const char* key, std::string& target, const std::string& path, bool multiline = false, bool empty = false)
            {
                const auto found = object.find(key);
                if (found == object.end()) return;
                if (!ValidText(*found, multiline, empty)) { 
                    Error(path + key); 
                    return; 
                }
                target = found->get<std::string>();
            }

            template<class Callback>
            void Entries(const Json& object, const char* key, const std::string& path, Callback callback)
            {
                const auto found = object.find(key);
                if (found == object.end()) return;
                if (!found->is_object()) { 
                    Error(path + key); 
                    return; 
                }
                for (const auto& [id, entry] : found->items()) {
                    const auto field = path + key + "/" + id + "/";
                    if (!entry.is_object() || !callback(id, entry, field)) { 
                        Error(field); 
                    }
                }
            }

            void Apply(const Json& document, ModSchema& mod, Messages& ui)
            {
                Fields(document, { "version", "title", "description", "groups", "settings", "hotkeys", "actions", "ui" }, "");
                Text(document, "title", mod.title, "");
                Text(document, "description", mod.description, "", true, true);
                Entries(document, "groups", "", [&](const auto& id, const auto& entry, const auto& field) {
                    auto group = std::ranges::find(mod.groups, id, &SettingsGroup::id);
                    if (group == mod.groups.end()) return false;
                    Fields(entry, { "label" }, field);
                    Text(entry, "label", group->label, field);
                    return true;
                });
                Entries(document, "settings", "", [&](const auto& id, const auto& entry, const auto& field) {
                    auto* setting = mod.FindSetting(id);
                    if (!setting) return false;
                    Fields(entry, { "label", "hint", "optionLabels" }, field);
                    Text(entry, "label", setting->label, field);
                    Text(entry, "hint", setting->hint, field, true, true);
                    if (auto options = entry.find("optionLabels"); options != entry.end()) {
                        auto* definition = std::get_if<EnumDefinition>(&setting->definition);
                        if (!definition || !options->is_object()) Error(field + "optionLabels");
                        else for (const auto& [value, label] : options->items()) {
                            auto option = std::ranges::find(definition->options, value, &EnumOption::value);
                            if (option == definition->options.end() || !ValidText(label, false, false)) {
                                Error(field + "optionLabels/" + value);
                            } else {
                                option->label = label.template get<std::string>();
                            }
                        }
                    }
                    return true;
                });
                Entries(document, "hotkeys", "", [&](const auto& id, const auto& entry, const auto& field) {
                    auto hotkey = std::ranges::find(mod.hotkeys, id, &HotkeyDefinition::id);
                    if (hotkey == mod.hotkeys.end()) return false;
                    Fields(entry, { "label" }, field);
                    Text(entry, "label", hotkey->label, field);
                    return true;
                });
                Entries(document, "actions", "", [&](const auto& id, const auto& entry, const auto& field) {
                    auto* action = mod.FindAction(id);
                    if (!action) return false;
                    Fields(entry, { "label", "hint", "confirmation" }, field);
                    Text(entry, "label", action->label, field);
                    Text(entry, "hint", action->hint, field, true, true);
                    if (action->confirmation.empty()) {
                        if (entry.contains("confirmation")) {
                            Error(field + "confirmation (action has no confirmation)");
                        }
                    } else {
                        Text(entry, "confirmation", action->confirmation, field, true);
                    }
                    return true;
                });
                if (auto messages = document.find("ui"); messages != document.end()) {
                    if (mod.id != "osfsettings" || !messages->is_object()) { Error("ui"); return; }
                    for (const auto& [key, value] : messages->items()) {
                        auto target = ui.find(key);
                        if (target == ui.end() || !ValidText(value, true, false)) { 
                            Error("ui/" + key); 
                            continue; 
                        }
                        const auto placeholders = Placeholders(value.get_ref<const std::string&>());
                        if (!placeholders || placeholders != Placeholders(target->second)) { 
                            Error("ui/" + key + " (placeholders)"); 
                            continue; 
                        }
                        target->second = value.get<std::string>();
                    }
                }
            }
        };

        std::atomic<std::shared_ptr<const Catalog>>& Current()
        {
            static auto* current = new std::atomic<std::shared_ptr<const Catalog>>(std::make_shared<Catalog>());
            return *current;
        }
    }

    std::string NormalizeLanguage(std::string_view language)
    {
        if (language.empty() || language.size() > 32 || language.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != language.npos) return "en";
        std::string result(language);
        for (auto& c : result) {
            if (c >= 'A' && c <= 'Z') {
                c += 'a' - 'A';
            }
        }
        return result;
    }

    std::string Format(std::string_view text, std::initializer_list<std::pair<std::string_view, std::string_view>> arguments)
    {
        std::string result;
        for (std::size_t i = 0; i < text.size();) {
            const auto end = text[i] == '{' ? text.find('}', i + 1) : text.npos;
            if (end != text.npos) {
                const auto name = text.substr(i + 1, end - i - 1);
                const auto argument = std::ranges::find(arguments, name, &std::pair<std::string_view, std::string_view>::first);
                if (argument != arguments.end()) { 
                    result += argument->second; 
                    i = end + 1; 
                    continue; 
                }
            }
            result += text[i++];
        }
        return result;
    }

    Catalog::Catalog() : m_ui(Json::parse(English).at("ui").get<Messages>()) {}

    Catalog::Catalog(const std::filesystem::path& directory, std::string_view language, std::span<const ModSettings> mods) : Catalog()
    {
        m_language = NormalizeLanguage(language);
        for (const auto& mod : mods) {
            m_schemas.emplace(mod.schema.id, mod.schema);
        }
        // Interface messages must work even when the OSF hotkey schema is absent.
        const bool interfaceOnly = m_schemas.try_emplace("osfsettings", ModSchema{ .id = "osfsettings" }).second;
        for (const auto& locale : m_language == "en" ? std::vector<std::string>{ "en" } : std::vector<std::string>{ "en", m_language }) {
            for (auto& [id, schema] : m_schemas) {
                const auto file = directory / locale / (id + ".json");
                try {
                    if (!std::filesystem::exists(file)) continue;
                    std::ifstream input(file, std::ios::binary);
                    if (!input) { m_errors.push_back({ file, "cannot open translation catalog" }); continue; }
                    std::vector<std::set<std::string>> keys;
                    auto document = Json::parse(input, [&](int depth, Json::parse_event_t event, Json& value) {
                        if (event == Json::parse_event_t::object_start) {
                            if (keys.size() <= static_cast<std::size_t>(depth + 1)) keys.resize(depth + 2);
                            keys[depth + 1].clear();
                        } else if (event == Json::parse_event_t::key && !keys[depth].insert(value.get<std::string>()).second) {
                            throw std::runtime_error("duplicate translation field: " + value.get<std::string>());
                        }
                        return true;
                    });
                    if (!document.is_object()) { m_errors.push_back({ file, "translation catalog must be an object" }); continue; }
                    if (document.contains("version") && (!document["version"].is_number_integer() || document["version"] != 1)) {
                        m_errors.push_back({ file, "translation version must be integer 1" }); continue;
                    }
                    if (interfaceOnly && id == "osfsettings") {
                        for (const auto* field : { "title", "description", "groups", "settings", "hotkeys", "actions" }) {
                            document.erase(field);
                        }
                    }
                    Overlay{ file, m_errors }.Apply(document, schema, m_ui);
                } catch (const std::exception& error) { m_errors.push_back({ file, error.what() }); }
            }
        }
    }

    void Catalog::Apply(ModSchema& schema) const
    {
        if (const auto found = m_schemas.find(schema.id); found != m_schemas.end()) {
            schema = found->second;
        }
    }

    std::shared_ptr<const Catalog> Get() { return Current().load(); }
    void Publish(std::shared_ptr<const Catalog> catalog) { Current().store(std::move(catalog)); }
    std::string Text(std::string_view key)
    {
        const auto catalog = Get();
        const auto found = catalog->UI().find(key);
        return found == catalog->UI().end() ? std::string(key) : found->second;
    }
    std::string Text(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> arguments) { return Format(Text(key), arguments); }
}
