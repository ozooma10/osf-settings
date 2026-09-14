#pragma once

namespace OSFSettings
{
    class Runtime
    {
    public:
        static Runtime& Get();
        bool Initialize();
    private:
        bool m_initialized{ false };
    };
}