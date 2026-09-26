#pragma once

namespace OSFSettings
{
    enum class SettingsError
    {
        None,
        NotReady,
        InvalidArgument,
        UnknownMod,
        UnknownSetting,
        TypeMismatch,
        InvalidValue,
        SaveFailed,
        UnknownSubscription,
        InternalError,
        UnknownSuppression,
        UnknownHotkey,
        AlreadyRegistered
    };
}
