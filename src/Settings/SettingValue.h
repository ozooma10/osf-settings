#pragma once

#include <map>
#include <string>

namespace OSFSettings
{
    // A setting has exactly one supported value type in this milestone.
    using SettingValue = bool;
    using SettingValues = std::map<std::string, SettingValue, std::less<>>;
}
