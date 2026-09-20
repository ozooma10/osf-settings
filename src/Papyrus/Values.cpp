#include "Values.h"

namespace OSFSettings::Papyrus
{
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
