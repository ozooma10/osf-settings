#include "Values.h"

namespace OSFSettings::Papyrus
{
    std::string FoldIdentifier(std::string_view value)
    {
        std::string result(value);
        for (auto& c : result) {
            if (c >= 'A' && c <= 'Z') {
                c += 'a' - 'A';
            }
        }
        return result;
}

    std::expected<std::pair<std::string, std::string>, SettingsError> Values::Resolve(std::string_view mod, std::string_view key, SettingValue* value) const
    {
        if (!m_service.IsReady()) return std::unexpected(SettingsError::NotReady);
        const auto modId = FoldIdentifier(mod), keyId = FoldIdentifier(key);
        // Papyrus interns identifiers without case sensitivity. Resolve only unique names.
        const auto mods = m_service.Snapshot();
        for (const auto& item : mods) {
            if (item.schema.id != modId) continue;
            const SettingDefinition* found{};
            for (const auto& group : item.schema.groups) {
                for (const auto& control : group.controls) {
                    const auto* valueSetting = std::get_if<SettingDefinition>(&control);
                    if (!valueSetting) continue;
                    const auto& setting = *valueSetting;
                    if (FoldIdentifier(setting.key) != keyId) continue;
                    if (found) return std::unexpected(SettingsError::InvalidArgument);
                    found = &setting;
                }
            }
            if (!found) return std::unexpected(SettingsError::UnknownSetting);
            if (auto* option = value ? std::get_if<EnumValue>(value) : nullptr) {
                if (const auto* definition = std::get_if<EnumDefinition>(&found->definition)) {
                    const auto id = FoldIdentifier(option->value);
                    const EnumOption* match{};
                    for (const auto& candidate : definition->options) {
                        if (FoldIdentifier(candidate.value) != id) continue;
                        if (match) return std::unexpected(SettingsError::InvalidArgument);
                        match = &candidate;
                    }
                    if (!match) return std::unexpected(SettingsError::InvalidValue);
                    option->value = match->value;
                }
            }
            return std::pair{ item.schema.id, found->key };
        }
        return std::unexpected(SettingsError::UnknownMod);
    }

    const char* ErrorName(SettingsError error) noexcept
    {
        switch (error) {
        case SettingsError::None: return "Ok";
        case SettingsError::NotReady: return "NotReady";
        case SettingsError::InvalidArgument: return "InvalidArgument";
        case SettingsError::UnknownMod: return "UnknownMod";
        case SettingsError::UnknownSetting: return "UnknownSetting";
        case SettingsError::TypeMismatch: return "TypeMismatch";
        case SettingsError::InvalidValue: return "InvalidValue";
        case SettingsError::SaveFailed: return "SaveFailed";
        case SettingsError::UnknownSubscription: return "UnknownSubscription";
        case SettingsError::UnknownHotkey: return "UnknownHotkey";
        default: return "InternalError";
        }
    }

    bool Report(std::string_view function, std::string_view mod, std::string_view key, SettingsError error)
    {
        if (error == SettingsError::None) return true;
        REX::WARN("Papyrus {}({}/{}): {}", function, mod, key, ErrorName(error));
        return false;
    }
}
