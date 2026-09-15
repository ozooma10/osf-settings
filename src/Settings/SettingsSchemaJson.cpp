#include "SettingsJson.h"

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
            Require(mod.id != "." && mod.id != ".." && std::ranges::all_of(mod.id, [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
            }), "mod id must use lowercase ASCII letters, digits, dots, underscores, or hyphens");
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
                    Require(settingKeys.insert(setting.key).second, "duplicate setting key: " + setting.key);
                    const auto type = RequiredText(sourceSetting, "type");
                    Require(type == "bool" || type == "int" || type == "float", "only types bool, int, and float are supported: " + setting.key);
                    std::string defaultError = "default must be a boolean: ";
                    if (type == "int") {
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
                        Require(!definition.minimum || !definition.maximum || *definition.minimum <= *definition.maximum, "min must not exceed max: " + setting.key);
                        setting.definition = definition;
                    }
                    const auto value = sourceSetting.find("default");
                    const auto decoded = value != sourceSetting.end() ? DecodeValue(*value, setting) : std::nullopt;
                    Require(decoded && IsValidValue(setting, *decoded), defaultError + setting.key);
                    std::visit([&](auto& definition) {
                        definition.defaultValue = std::get<decltype(definition.defaultValue)>(*decoded);
                    }, setting.definition);
                    setting.label = OptionalText(sourceSetting, "label", setting.key);
                    setting.hint = OptionalText(sourceSetting, "hint");
                    group.settings.push_back(std::move(setting));
                }
                mod.groups.push_back(std::move(group));
            }
            return mod;
        } catch (const std::exception& exception) {
            error = exception.what();
            return std::nullopt;
        }
    }
}
