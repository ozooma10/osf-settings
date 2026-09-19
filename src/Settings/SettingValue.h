#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>

namespace OSFSettings
{
    struct KeyBinding
    {
        static constexpr std::uint32_t Unbound = 0xFF;
        std::uint32_t keyCode{ Unbound }; // Starfield keyboard ButtonEvent::idCode (Win32 VK).
        bool operator==(const KeyBinding&) const = default;
    };

    struct EnumValue
    {
        std::string value;
        bool operator==(const EnumValue&) const = default;
    };

    using SettingValue = std::variant<bool, std::int64_t, double, std::string, KeyBinding, EnumValue>;
    using SettingValues = std::map<std::string, SettingValue, std::less<>>;
}
