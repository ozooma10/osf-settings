#pragma once

#include "Settings/SettingsStore.h"

namespace OSFSettings
{
    class Runtime
    {
    public:
        static Runtime& Get();
        bool Initialize();
        const SettingsStore& Settings() const { return m_settings; }
        SettingsStore::SetResult SetValue(std::string_view mod, std::string_view key, SettingValue value);
    private:
        SettingsStore m_settings;
        bool m_initialized{ false };
    };
}
