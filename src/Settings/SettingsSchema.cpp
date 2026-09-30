#include "SettingsSchema.h"
#include "Input/KeyNames.h"

#include <algorithm>
#include <cmath>
#include <utility>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#undef ERROR

namespace OSFSettings
{
    bool IsValidString(std::string_view text, std::uint32_t maxLength)
    {
        if (text.size() > maxLength || !std::in_range<int>(text.size())) return false;
        if (text.empty()) return true;

        // UTF-16 needs at most one code unit per UTF-8 byte.
        std::wstring wide(text.size(), L'\0');
        const auto length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), wide.data(), static_cast<int>(wide.size()));
        if (length == 0) return false;
        wide.resize(length);

        return std::ranges::none_of(wide, [](wchar_t code) {
            // Single-line text: no NUL, C0/C1 controls, or Unicode line/paragraph separators.
            return code < 0x20 || (code >= 0x7F && code <= 0x9F) || code == 0x2028 || code == 0x2029;
        });
    }

    bool IsValidModId(std::string_view id)
    {
        if (id.empty() || id.size() > 128 || id == "." || id == "..") return false;
        const auto base = id.substr(0, id.find('.'));
        if (base == "con" || base == "prn" || base == "aux" || base == "nul" ||
            (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) && base[3] >= '0' && base[3] <= '9')) return false;
        return std::ranges::all_of(id, [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        });
    }

    bool IsValidIdentifier(std::string_view id)
    {
        return !id.empty() && std::ranges::all_of(id, [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        });
    }

    std::string FoldAscii(std::string_view text)
    {
        std::string result(text);
        for (auto& c : result) {
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        }
        return result;
    }

    SettingValue SettingDefinition::DefaultValue() const
    {
        return std::visit([](const auto& data) -> SettingValue { return data.defaultValue; }, definition);
    }

    bool IsValidValue(const SettingDefinition& setting, const SettingValue& value)
    {
        if (const auto* definition = std::get_if<StringDefinition>(&setting.definition)) {
            const auto* text = std::get_if<std::string>(&value);
            return text && IsValidString(*text, definition->maxLength);
        }
        if (const auto* definition = std::get_if<IntDefinition>(&setting.definition)) {
            const auto* integer = std::get_if<std::int64_t>(&value);
            return integer && (!definition->minimum || *integer >= *definition->minimum) && (!definition->maximum || *integer <= *definition->maximum);
        }
        if (const auto* definition = std::get_if<FloatDefinition>(&setting.definition)) {
            const auto* number = std::get_if<double>(&value);
            return number && std::isfinite(*number) && (!definition->minimum || *number >= *definition->minimum) && (!definition->maximum || *number <= *definition->maximum);
        }
        if (const auto* definition = std::get_if<EnumDefinition>(&setting.definition)) {
            const auto* option = std::get_if<EnumValue>(&value);
            return option && std::ranges::find(definition->options, option->value, &EnumOption::value) != definition->options.end();
        }
        if (const auto* definition = std::get_if<KeyDefinition>(&setting.definition)) {
            const auto* key = std::get_if<KeyBinding>(&value);
            return key && (key->keyCode == KeyBinding::Unbound ? definition->allowUnbound :
                IsBindableKey(key->keyCode) || (definition->allowMouse && IsMouseKey(key->keyCode)));
        }
        return std::holds_alternative<BoolDefinition>(setting.definition) && std::holds_alternative<bool>(value);
    }

    namespace
    {
        template <class T, class Id>
        const T* FindControl(const std::vector<SettingsGroup>& groups, Id id, std::string_view wanted)
        {
            for (const auto& group : groups) {
                for (const auto& control : group.controls) {
                    if (const auto* found = std::get_if<T>(&control); found && found->*id == wanted) return found;
                }
            }
            return nullptr;
        }
    }

    const SettingDefinition* ModSchema::FindSetting(std::string_view key) const
    {
        return FindControl<SettingDefinition>(groups, &SettingDefinition::key, key);
    }

    SettingDefinition* ModSchema::FindSetting(std::string_view key)
    {
        return const_cast<SettingDefinition*>(static_cast<const ModSchema&>(*this).FindSetting(key));
    }

    const ActionDefinition* ModSchema::FindAction(std::string_view actionId) const
    {
        return FindControl<ActionDefinition>(groups, &ActionDefinition::id, actionId);
    }

    SettingValues ModSchema::DefaultValues() const
    {
        SettingValues values;
        for (const auto& group : groups) {
            for (const auto& control : group.controls) {
                if (const auto* setting = std::get_if<SettingDefinition>(&control)) values.emplace(setting->key, setting->DefaultValue());
            }
        }
        return values;
    }

    std::optional<SettingValue> ModSettings::GetValue(std::string_view key) const
    {
        if (const auto saved = values.find(key); saved != values.end()) return saved->second;
        if (const auto* setting = schema.FindSetting(key)) return setting->DefaultValue();
        return std::nullopt;
    }

    SettingValues ModSettings::ResolvedValues() const
    {
        auto resolved = schema.DefaultValues();
        for (const auto& [key, value] : values) resolved.insert_or_assign(key, value);
        return resolved;
    }

    ActionDefinition* ModSchema::FindAction(std::string_view actionId)
    {
        return const_cast<ActionDefinition*>(static_cast<const ModSchema&>(*this).FindAction(actionId));
    }
}
