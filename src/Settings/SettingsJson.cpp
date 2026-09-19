#include "SettingsJson.h"

#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>

namespace OSFSettings::SettingsJson
{
    std::optional<std::int64_t> DecodeInteger(const nlohmann::json& value)
    {
        if (value.is_number_unsigned()) {
            const auto integer = value.get<std::uint64_t>();
            if (integer > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return std::nullopt;
            return static_cast<std::int64_t>(integer);
        }
        if (value.is_number_integer()) {
            return value.get<std::int64_t>();
        }
        return std::nullopt;
    }

    std::optional<double> DecodeFloat(const nlohmann::json& value)
    {
        if (!value.is_number()) return std::nullopt;
        const auto number = value.get<double>();
        return std::isfinite(number) ? std::optional(number) : std::nullopt;
    }

    std::optional<SettingValue> DecodeValue(const nlohmann::json& value, const SettingDefinition& setting)
    {
        if (std::holds_alternative<StringDefinition>(setting.definition) && value.is_string()) {
            return value.get<std::string>();
        }
        if (std::holds_alternative<BoolDefinition>(setting.definition) && value.is_boolean()) {
            return value.get<bool>();
        }
        if (std::holds_alternative<IntDefinition>(setting.definition)) {
            if (const auto integer = DecodeInteger(value)) {
                return *integer;
            }
        }
        if (std::holds_alternative<FloatDefinition>(setting.definition)) {
            if (const auto number = DecodeFloat(value)) {
                return *number;
            }
        }
        if (std::holds_alternative<EnumDefinition>(setting.definition) && value.is_string()) {
            return EnumValue{ value.get<std::string>() };
        }
        if (std::holds_alternative<KeyDefinition>(setting.definition)) {
            if (const auto code = DecodeInteger(value); code && *code >= 0 && *code <= KeyBinding::Unbound) {
                return KeyBinding{ static_cast<std::uint32_t>(*code) };
            }
        }
        return std::nullopt;
    }
}
