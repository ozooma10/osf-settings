#pragma once

#include <string_view>

namespace OSFSettings::NativeHotkeys
{
    bool Install();
    std::string_view GetMenu(std::string_view action);
}
