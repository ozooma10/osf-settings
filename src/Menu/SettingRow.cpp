#include "SettingRow.h"
#include "FloatSlider.h"

namespace OSFSettings
{
    SettingRow MakeSettingRow(const SettingDefinition& setting, const SettingValue& value)
    {
        SettingRow row;
        auto& fields = row.fields;
        fields.emplace_back("requiresRestart", setting.requiresRestart);
        if (const auto* boolean = std::get_if<BoolDefinition>(&setting.definition)) {
            fields.emplace_back("type", std::string("bool"));
            fields.emplace_back("value", std::get<bool>(value));
            fields.emplace_back("defaultValue", boolean->defaultValue);
            fields.emplace_back("editable", true);
        } else if (const auto* integer = std::get_if<IntDefinition>(&setting.definition)) {
            fields.emplace_back("type", std::string("int"));
            fields.emplace_back("value", std::to_string(std::get<std::int64_t>(value)));
            fields.emplace_back("defaultValue", std::to_string(integer->defaultValue));
            if (integer->minimum) fields.emplace_back("minimum", std::to_string(*integer->minimum));
            if (integer->maximum) fields.emplace_back("maximum", std::to_string(*integer->maximum));
            // AS3 Number must preserve both bounds; BSSlider stores its range as uint32.
            fields.emplace_back("editable", integer->minimum && integer->maximum && *integer->minimum < *integer->maximum &&
                *integer->minimum >= -kMaxSafeInteger && *integer->maximum <= kMaxSafeInteger &&
                *integer->maximum - *integer->minimum <= 4294967295LL);
        } else if (const auto* floating = std::get_if<FloatDefinition>(&setting.definition)) {
            fields.emplace_back("type", std::string("float"));
            fields.emplace_back("value", std::get<double>(value));
            fields.emplace_back("defaultValue", floating->defaultValue);
            if (floating->minimum) fields.emplace_back("minimum", *floating->minimum);
            if (floating->maximum) fields.emplace_back("maximum", *floating->maximum);
            const auto slider = MakeFloatSlider(*floating);
            fields.emplace_back("editable", slider.has_value());
            fields.emplace_back("decimals", static_cast<double>(slider ? slider->decimals : -1));
            if (slider) {
                fields.emplace_back("sliderMinimum", static_cast<double>(slider->minimum));
                fields.emplace_back("sliderMaximum", static_cast<double>(slider->maximum));
                fields.emplace_back("sliderStep", static_cast<double>(slider->step));
                fields.emplace_back("sliderScale", static_cast<double>(slider->scale));
                fields.emplace_back("sliderSteps", static_cast<double>(slider->steps));
            }
        } else if (const auto* text = std::get_if<StringDefinition>(&setting.definition)) {
            fields.emplace_back("type", std::string("string"));
            fields.emplace_back("value", std::get<std::string>(value));
            fields.emplace_back("defaultValue", text->defaultValue);
            fields.emplace_back("maxLength", static_cast<double>(text->maxLength));
            fields.emplace_back("editable", true);
        } else if (const auto* binding = std::get_if<KeyDefinition>(&setting.definition)) {
            fields.emplace_back("type", std::string("key"));
            fields.emplace_back("value", static_cast<double>(std::get<KeyBinding>(value).keyCode));
            fields.emplace_back("defaultValue", static_cast<double>(binding->defaultValue.keyCode));
            fields.emplace_back("editable", true);
            fields.emplace_back("allowUnbound", binding->allowUnbound);
            fields.emplace_back("allowMouse", binding->allowMouse);
        } else if (const auto* enumeration = std::get_if<EnumDefinition>(&setting.definition)) {
            fields.emplace_back("type", std::string("enum"));
            fields.emplace_back("value", std::get<EnumValue>(value).value);
            fields.emplace_back("defaultValue", enumeration->defaultValue.value);
            fields.emplace_back("editable", enumeration->options.size() > 1);
            row.options = enumeration->options;
        }
        return row;
    }
}
