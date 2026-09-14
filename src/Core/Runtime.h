#pragma once

#include "Settings/SettingsStore.h"

namespace OSFSettings
{
    class Runtime
    {
    public:
        static Runtime& Get();
        bool Initialize();
    private:
        SettingsStore m_settings;
        bool m_initialized{ false };
    };
}
