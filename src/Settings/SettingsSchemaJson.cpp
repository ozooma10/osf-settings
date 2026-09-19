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

        std::string RequiredText(const nlohmann::json& object, const char* key)
        {
            const auto field = object.find(key);
            Require(field != object.end() && field->is_string(), std::string(key) + " must be a string");
            auto value = field->get<std::string>();
            Require(!value.empty(), std::string(key) + " must not be empty");
            return value;
        }

        std::optional<std::int64_t> ReadInteger(const nlohmann::json& object, const char* key)
        {
            const auto field = object.find(key);
            if (field == object.end()) return std::nullopt;
            const auto value = DecodeInteger(*field);
            Require(value.has_value(), std::string(key) + " must be a signed 64-bit integer");
            return value;
        }

        std::optional<double> ReadFloat(const nlohmann::json& object, const char* key)
        {
            const auto field = object.find(key);
            if (field == object.end()) return std::nullopt;
            const auto value = DecodeFloat(*field);
            Require(value.has_value(), std::string(key) + " must be a finite number");
            return value;
        }

        std::string OptionalText(const nlohmann::json& object, const char* key, const std::string& fallback = {})
        {
            const auto field = object.find(key);
            if (field == object.end()) return fallback;
            Require(field->is_string(), std::string(key) + " must be a string");
            return field->get<std::string>();
        }

        std::optional<SettingValue> DecodeDefault(const nlohmann::json& value, const SettingDefinition& setting)
        {
            if (std::holds_alternative<KeyDefinition>(setting.definition) && value.is_string()) {
                const auto code = KeyCodeFromName(value.get_ref<const std::string&>());
                if (!code) return std::nullopt;
                return KeyBinding{ *code };
            }
            return DecodeValue(value, setting);
        }
    }

    std::optional<ModSchema> ParseSchema(const nlohmann::json& document, std::string& error)
    {
        error.clear();
        try {
            Require(document.is_object(), "schema must be an object");
            const auto version = document.find("schemaVersion");
            Require(version != document.end() && version->is_number_integer() && *version == 1,  "schemaVersion must be the integer 1");

            ModSchema mod;
            mod.id = RequiredText(document, "id");
            Require(IsValidModId(mod.id), "mod id must use lowercase ASCII letters, digits, dots, underscores, or hyphens");
            mod.title = OptionalText(document, "title", mod.id);
            mod.description = OptionalText(document, "description");

            const auto groups = document.find("groups");
            Require(groups != document.end() && groups->is_array(), "groups must be an array");
            std::set<std::string> groupIds;
            std::set<std::string> settingKeys;
            for (const auto& sourceGroup : *groups) {
                Require(sourceGroup.is_object(), "each group must be an object");
                SettingsGroup group;
                group.id = RequiredText(sourceGroup, "id");
                Require(groupIds.insert(group.id).second, "duplicate group id: " + group.id);
                group.label = OptionalText(sourceGroup, "label", group.id);

                const auto settings = sourceGroup.find("settings");
                Require(settings != sourceGroup.end() && settings->is_array(), "group settings must be an array");
                for (const auto& sourceSetting : *settings) {
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
                        Require(options != sourceSetting.end() && options->is_array() && !options->empty(), "options must be a non-empty array: " + setting.key);
                        const auto labels = sourceSetting.find("optionLabels");
                        Require(labels == sourceSetting.end() || (labels->is_array() && labels->size() == options->size()), "optionLabels must be an array with one label per option: " + setting.key);
                        std::set<std::string> optionValues;
                        for (std::size_t index = 0; index < options->size(); ++index) {
                            const auto& sourceOption = (*options)[index];
                            Require(sourceOption.is_string() && !sourceOption.get_ref<const std::string&>().empty(), "each option must be a non-empty string: " + setting.key);
                            EnumOption option;
                            option.value = sourceOption.get<std::string>();
                            Require(option.value.find('\0') == std::string::npos, "option value must not contain NUL: " + setting.key);
                            Require(optionValues.insert(option.value).second, "duplicate option: " + setting.key + " / " + option.value);
                            option.label = option.value;
                            if (labels != sourceSetting.end()) {
                                const auto& label = (*labels)[index];
                                Require(label.is_string(), "each option label must be a string: " + setting.key);
                                if (!label.get_ref<const std::string&>().empty()) option.label = label.get<std::string>();
                            }
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
                        Require(groupIds.contains(hotkey.group), "unknown hotkey group: " + hotkey.group);
                    } else {
                        hotkey.group = mod.groups.empty() ? "general" : mod.groups.front().id;
                    }
                    mod.hotkeys.push_back(std::move(hotkey));
                }
            }
            if (mod.groups.empty() && !mod.hotkeys.empty()) {
                mod.groups.push_back({ "general", "General", {} });
            }
            return mod;
        } catch (const std::exception& exception) {
            error = exception.what();
            return std::nullopt;
        }
    }
}
