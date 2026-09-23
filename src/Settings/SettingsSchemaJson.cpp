#include "SettingsJson.h"
#include "Input/KeyNames.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>
#include <nlohmann/json.hpp>

namespace OSFSettings::SettingsJson
{
    namespace
    {
        void Require(bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        }

        std::string RequiredText(const nlohmann::ordered_json& object, const char* key)
        {
            const auto field = object.find(key);
            Require(field != object.end() && field->is_string(), std::string(key) + " must be a string");
            auto value = field->get<std::string>();
            Require(!value.empty(), std::string(key) + " must not be empty");
            return value;
        }

        std::optional<std::int64_t> ReadInteger(const nlohmann::ordered_json& object, const char* key)
        {
            const auto field = object.find(key);
            if (field == object.end()) return std::nullopt;
            const auto value = DecodeInteger(*field);
            Require(value.has_value(), std::string(key) + " must be a signed 64-bit integer");
            return value;
        }

        std::optional<double> ReadFloat(const nlohmann::ordered_json& object, const char* key)
        {
            const auto field = object.find(key);
            if (field == object.end()) return std::nullopt;
            const auto value = DecodeFloat(*field);
            Require(value.has_value(), std::string(key) + " must be a finite number");
            return value;
        }

        std::string OptionalText(const nlohmann::ordered_json& object, const char* key, const std::string& fallback = {})
        {
            const auto field = object.find(key);
            if (field == object.end()) return fallback;
            Require(field->is_string(), std::string(key) + " must be a string");
            return field->get<std::string>();
        }

        std::optional<SettingValue> DecodeDefault(const nlohmann::ordered_json& value, const SettingDefinition& setting)
        {
            if (std::holds_alternative<KeyDefinition>(setting.definition) && value.is_string()) {
                const auto code = KeyCodeFromName(value.get_ref<const std::string&>());
                if (!code) return std::nullopt;
                return KeyBinding{ *code };
            }
            return DecodeValue(value, setting);
        }
    }

    std::optional<ModSchema> ParseSchema(std::istream& input, std::string_view modId, std::string& error)
    {
        error.clear();
        try {
            std::set<std::string> groupNames;
            std::set<std::string> optionValues;
            bool inGroups{};
            bool inOptions{};
            const auto document = nlohmann::ordered_json::parse(input,
                [&](int depth, nlohmann::ordered_json::parse_event_t event, nlohmann::ordered_json& value) {
                    if (event == nlohmann::ordered_json::parse_event_t::key) {
                        if (depth == 1) {
                            inGroups = value == "groups";
                        } else if (depth == 2 && inGroups) {
                            const auto& name = value.get_ref<const std::string&>();
                            Require(groupNames.insert(name).second, "duplicate group name: " + name);
                        } else if (depth == 4 && inGroups) {
                            inOptions = value == "options";
                            if (inOptions) {
                                optionValues.clear();
                            }
                        } else if (depth == 5 && inGroups && inOptions) {
                            const auto& option = value.get_ref<const std::string&>();
                            Require(optionValues.insert(option).second, "duplicate option: " + option);
                        }
                    }
                    return true;
                });
            return ParseSchema(document, modId, error);
        } catch (const std::exception& exception) {
            error = exception.what();
            return std::nullopt;
        }
    }

    std::optional<ModSchema> ParseSchema(const nlohmann::ordered_json& document, std::string_view modId, std::string& error)
    {
        error.clear();
        try {
            Require(document.is_object(), "schema must be an object");
            const auto version = document.find("schemaVersion");
            Require(version == document.end() || (version->is_number_integer() && *version == 1), "schemaVersion must be the integer 1 when present");

            ModSchema mod;
            mod.id = modId;
            Require(IsValidModId(mod.id), "mod id must use lowercase ASCII letters, digits, dots, underscores, or hyphens");
            if (document.contains("id")) {
                Require(RequiredText(document, "id") == mod.id, "schema id must match the filename stem");
            }
            mod.title = OptionalText(document, "title", mod.id);
            mod.description = OptionalText(document, "description");

            const auto groups = document.find("groups");
            Require(groups != document.end() && groups->is_object(), "groups must be an object");
            std::set<std::string> settingKeys;
            for (const auto& [name, settings] : groups->items()) {
                Require(!name.empty(), "group name must not be empty");
                Require(name.find('\0') == std::string::npos, "group name must not contain NUL");
                SettingsGroup group;
                group.id = name;
                group.label = name;

                Require(settings.is_array(), "group settings must be an array: " + name);
                for (const auto& sourceSetting : settings) {
                    Require(sourceSetting.is_object(), "each setting must be an object");
                    SettingDefinition setting;
                    setting.key = RequiredText(sourceSetting, "key");
                    Require(setting.key.find('\0') == std::string::npos, "setting key must not contain NUL");
                    Require(settingKeys.insert(setting.key).second, "duplicate setting key: " + setting.key);
                    const auto type = RequiredText(sourceSetting, "type");
                    Require(type == "bool" || type == "int" || type == "float" || type == "enum" || type == "key" || type == "string", "only types bool, int, float, enum, key, and string are supported: " + setting.key);
                    std::string defaultError = "default must be a boolean: ";
                    if (type == "string") {
                        defaultError = "default must be valid single-line UTF-8 within maxLength bytes: ";
                        StringDefinition definition;
                        const auto limit = ReadInteger(sourceSetting, "maxLength").value_or(StringDefinition::DefaultMaxLength);
                        Require(limit >= 1 && limit <= StringDefinition::MaxLength, "maxLength must be an integer from 1 to 4096 UTF-8 bytes: " + setting.key);
                        definition.maxLength = static_cast<std::uint32_t>(limit);
                        setting.definition = definition;
                    } else if (type == "int") {
                        defaultError = "default must be an integer within its bounds: ";
                        IntDefinition definition;
                        definition.minimum = ReadInteger(sourceSetting, "min");
                        definition.maximum = ReadInteger(sourceSetting, "max");
                        Require(!definition.minimum || !definition.maximum || *definition.minimum <= *definition.maximum, "min must not exceed max: " + setting.key);
                        setting.definition = definition;
                    } else if (type == "float") {
                        defaultError = "default must be a finite number within its bounds: ";
                        FloatDefinition definition;
                        definition.minimum = ReadFloat(sourceSetting, "min");
                        definition.maximum = ReadFloat(sourceSetting, "max");
                        definition.step = ReadFloat(sourceSetting, "step").value_or(0.1);
                        Require(definition.step > 0.0, "step must be positive: " + setting.key);
                        Require(!definition.minimum || !definition.maximum || *definition.minimum <= *definition.maximum, "min must not exceed max: " + setting.key);
                        setting.definition = definition;
                    } else if (type == "key") {
                        defaultError = "default must be a keyboard virtual-key integer or recognized key name (255/UNBOUND only with allowUnbound): ";
                        KeyDefinition definition;
                        const auto unbound = sourceSetting.find("allowUnbound");
                        Require(unbound == sourceSetting.end() || unbound->is_boolean(), "allowUnbound must be a boolean: " + setting.key);
                        definition.allowUnbound = unbound != sourceSetting.end() && unbound->get<bool>();
                        setting.definition = definition;
                    } else if (type == "enum") {
                        defaultError = "default must be a string matching an option: ";
                        EnumDefinition definition;
                        const auto options = sourceSetting.find("options");
                        Require(options != sourceSetting.end() && (options->is_array() || options->is_object()) && !options->empty(), "options must be a non-empty array or object: " + setting.key);
                        Require(!sourceSetting.contains("optionLabels"), "optionLabels is no longer supported; use an options object: " + setting.key);
                        std::set<std::string> optionValues;
                        for (const auto& [key, value] : options->items()) {
                            EnumOption option;
                            if (options->is_object()) {
                                option.value = key;
                                Require(value.is_string(), "each option label must be a string: " + setting.key);
                                option.label = value.get<std::string>();
                            } else {
                                Require(value.is_string(), "each option must be a non-empty string: " + setting.key);
                                option.value = value.get<std::string>();
                            }
                            Require(!option.value.empty(), "each option must be a non-empty string: " + setting.key);
                            Require(option.value.find('\0') == std::string::npos, "option value must not contain NUL: " + setting.key);
                            Require(optionValues.insert(option.value).second, "duplicate option: " + setting.key + " / " + option.value);
                            if (option.label.empty()) option.label = option.value;
                            definition.options.push_back(std::move(option));
                        }
                        setting.definition = std::move(definition);
                    }
                    const auto value = sourceSetting.find("default");
                    const auto decoded = value != sourceSetting.end() ? DecodeDefault(*value, setting) : std::nullopt;
                    Require(decoded && IsValidValue(setting, *decoded), defaultError + setting.key);
                    std::visit([&](auto& definition) {
                        definition.defaultValue = std::get<decltype(definition.defaultValue)>(*decoded);
                    }, setting.definition);
                    setting.label = OptionalText(sourceSetting, "label", setting.key);
                    setting.hint = OptionalText(sourceSetting, "hint");
                    if (const auto requirement = sourceSetting.find("requires"); requirement != sourceSetting.end()) {
                        Require(requirement->is_string() && *requirement == "restart", "requires must be \"restart\" when present: " + setting.key);
                        setting.requiresRestart = true;
                    }
                    group.settings.push_back(std::move(setting));
                }
                mod.groups.push_back(std::move(group));
            }
            if (const auto hotkeys = document.find("hotkeys"); hotkeys != document.end()) {
                Require(hotkeys->is_array(), "hotkeys must be an array");
                std::set<std::string> ids;
                for (const auto& source : *hotkeys) {
                    Require(source.is_object(), "each hotkey must be an object");
                    HotkeyDefinition hotkey;
                    hotkey.id = RequiredText(source, "id");
                    Require(hotkey.id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos, "hotkey id must use ASCII letters, digits, underscores, or hyphens");
                    auto folded = hotkey.id;
                    for (auto& ch : folded) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
                    Require(ids.insert(folded).second, "duplicate hotkey id: " + hotkey.id);
                    hotkey.label = RequiredText(source, "label");
                    Require(hotkey.label.find('\0') == std::string::npos, "hotkey label must not contain NUL");
                    if (source.contains("default")) {
                        hotkey.defaultKey = RequiredText(source, "default");
                        Require(hotkey.defaultKey->find('\0') == std::string::npos, "hotkey default must not contain NUL");
                    }
                    if (source.contains("menu")) {
                        hotkey.menu = RequiredText(source, "menu");
                        Require(hotkey.menu->find('\0') == std::string::npos, "hotkey menu must not contain NUL");
                    }
                    if (source.contains("group")) {
                        hotkey.group = RequiredText(source, "group");
                        Require(groups->contains(hotkey.group), "unknown hotkey group: " + hotkey.group);
                    } else {
                        hotkey.group = mod.groups.empty() ? "General" : mod.groups.front().id;
                    }
                    mod.hotkeys.push_back(std::move(hotkey));
                }
            }
            if (const auto actions = document.find("actions"); actions != document.end()) {
                Require(actions->is_array(), "actions must be an array");
                std::set<std::string> ids;
                for (const auto& source : *actions) {
                    Require(source.is_object(), "each action must be an object");
                    ActionDefinition action;
                    action.id = RequiredText(source, "id");
                    Require(action.id.size() <= 128 && action.id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos,
                        "action id must use 1-128 ASCII letters, digits, underscores, or hyphens");
                    auto folded = action.id;
                    for (auto& ch : folded) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
                    Require(ids.insert(folded).second, "duplicate action id: " + action.id);
                    action.label = RequiredText(source, "label");
                    action.hint = OptionalText(source, "hint");
                    action.confirmation = OptionalText(source, "confirmation");
                    Require(IsValidString(action.label, 256) && IsValidString(action.hint, 4096) && IsValidString(action.confirmation, 4096),
                        "action text must be single-line UTF-8 (label: 256 bytes; hint/confirmation: 4096 bytes)");
                    if (source.contains("confirmation")) Require(!action.confirmation.empty(), "confirmation must not be empty when present");
                    if (source.contains("group")) {
                        action.group = RequiredText(source, "group");
                        Require(groups->contains(action.group), "unknown action group: " + action.group);
                    } else {
                        action.group = mod.groups.empty() ? "General" : mod.groups.front().id;
                    }
                    Require(!source.contains("default"), "actions do not have a default value");
                    mod.actions.push_back(std::move(action));
                }
            }
            if (const auto menus = document.find("menus"); menus != document.end()) {
                Require(menus->is_array(), "menus must be an array");
                if (!menus->empty()) Require(mod.id.size() <= 128 && IsValidString(mod.title, 256),
                    "launcher owner id must fit 128 bytes and title must be single-line UTF-8 within 256 bytes");
                std::set<std::string> ids;
                for (const auto& source : *menus) {
                    Require(source.is_object(), "each menu must be an object");
                    MenuDefinition menu;
                    menu.id = RequiredText(source, "id");
                    menu.title = RequiredText(source, "title");
                    menu.description = OptionalText(source, "description");
                    menu.menu = RequiredText(source, "menu");
                    Require(IsValidString(menu.id, 256) && IsValidString(menu.title, 256) &&
                        IsValidString(menu.menu, 256) && IsValidString(menu.description, 4096),
                        "menu text must be single-line UTF-8 (id/title/menu: 256 bytes; description: 4096 bytes)");
                    Require(menu.menu != "OSFSettingsMenu", "a launcher cannot open OSFSettingsMenu itself");
                    Require(ids.insert(menu.id).second, "duplicate menu id: " + menu.id);
                    mod.menus.push_back(std::move(menu));
                }
            }
            if (mod.groups.empty() && (!mod.hotkeys.empty() || !mod.actions.empty())) {
                mod.groups.push_back({ "General", "General", {} });
            }
            return mod;
        } catch (const std::exception& exception) {
            error = exception.what();
            return std::nullopt;
        }
    }
}
