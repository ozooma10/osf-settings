#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>

namespace OSFSettings
{
    using SettingValue = std::variant<bool, std::int64_t, double>;
    using SettingValues = std::map<std::string, SettingValue, std::less<>>;
}
