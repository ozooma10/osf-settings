#include "Plugin.h"
#include "Runtime.h"

namespace OSFSettings::Plugin
{
    bool OnLoad()
    {
        return Runtime::Get().Initialize();
    }
}
