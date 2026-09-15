#pragma once

#include <map>
#include <string>
#include <variant>

namespace OSFSettings
{
    using SettingValue = std::variant<bool>;
    using SettingValues = std::map<std::string, SettingValue, std::less<>>;
}
