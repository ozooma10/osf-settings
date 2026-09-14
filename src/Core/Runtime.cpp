#include "Runtime.h"
#include "Utils/Paths.h"

namespace OSFSettings
{
    Runtime &Runtime::Get()
    {
        static Runtime instance;
        return instance;
    }

    bool Runtime::Initialize()
    {
        if(m_initialized) {
            return false;
        }
        if(!Paths::Initialize())
        {
            return false;
        }

        m_initialized = true;
        return true;
    }
}