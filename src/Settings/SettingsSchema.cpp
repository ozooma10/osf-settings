#include "SettingsSchema.h"

#include <algorithm>
#include <cmath>

namespace OSFSettings
{
    bool IsValidModId(std::string_view id)
    {
        return !id.empty() && id != "." && id != ".." && std::ranges::all_of(id, [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        });
    }

    SettingValue SettingDefinition::DefaultValue() const
    {
        return std::visit([](const auto& data) -> SettingValue { return data.defaultValue; }, definition);
    }

    bool IsValidValue(const SettingDefinition& setting, const SettingValue& value)
    {
        if (const auto* definition = std::get_if<IntDefinition>(&setting.definition)) {
            const auto* integer = std::get_if<std::int64_t>(&value);
            return integer && (!definition->minimum || *integer >= *definition->minimum) && (!definition->maximum || *integer <= *definition->maximum);
        }
        if (const auto* definition = std::get_if<FloatDefinition>(&setting.definition)) {
            const auto* number = std::get_if<double>(&value);
            return number && std::isfinite(*number) && (!definition->minimum || *number >= *definition->minimum) && (!definition->maximum || *number <= *definition->maximum);
        }
        if (const auto* definition = std::get_if<EnumDefinition>(&setting.definition)) {
            const auto* option = std::get_if<std::string>(&value);
            return option && std::ranges::find(definition->options, *option, &EnumOption::value) != definition->options.end();
        }
        return std::holds_alternative<BoolDefinition>(setting.definition) && std::holds_alternative<bool>(value);
    }

    const SettingDefinition* ModSchema::FindSetting(std::string_view key) const
    {
        for(const auto& group : groups)
        {
            for(const auto& setting : group.settings)
            {
                if (setting.key == key) {
                    return &setting;
                }
            }
        }
        return nullptr;
    }
}
