#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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

    struct ColorValue
    {
        static constexpr std::uint32_t OpaqueAlpha = 0xFF;
        std::uint32_t rgba{ OpaqueAlpha }; // 0xRRGGBBAA, the same byte order as the "#RRGGBBAA" text form.
        bool operator==(const ColorValue&) const = default;
        constexpr bool Opaque() const noexcept { return (rgba & 0xFFu) == OpaqueAlpha; }
    };

    // Accepts "#RRGGBB" (opaque) or "#RRGGBBAA", hex digits in either case. Nothing else: no "0x", no shorthand, no whitespace.
    constexpr std::optional<ColorValue> ParseColor(std::string_view text) noexcept
    {
        if ((text.size() != 7 && text.size() != 9) || text[0] != '#') return std::nullopt;
        std::uint32_t packed{};
        for (const auto c : text.substr(1)) {
            const auto byte = static_cast<std::uint32_t>(static_cast<unsigned char>(c));
            std::uint32_t digit{};
            if (byte >= '0' && byte <= '9') digit = byte - '0';
            else if (byte >= 'a' && byte <= 'f') digit = byte - 'a' + 10;
            else if (byte >= 'A' && byte <= 'F') digit = byte - 'A' + 10;
            else return std::nullopt;
            packed = (packed << 4) | digit;
        }
        return ColorValue{ text.size() == 7 ? (packed << 8) | ColorValue::OpaqueAlpha : packed };
    }

    // Uppercase "#RRGGBBAA"; opaque colors use "#RRGGBB" unless alpha is requested.
    inline std::string FormatColor(ColorValue color, bool includeOpaqueAlpha = false)
    {
        constexpr std::array digits{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
        const std::size_t count = color.Opaque() && !includeOpaqueAlpha ? 6 : 8;
        auto packed = count == 6 ? color.rgba >> 8 : color.rgba;
        std::string text(count + 1, '#');
        for (auto i = count; i > 0; --i, packed >>= 4) text[i] = digits[packed & 0xFu];
        return text;
    }

    using SettingValue = std::variant<bool, std::int64_t, double, std::string, KeyBinding, EnumValue, ColorValue>;
    using SettingValues = std::map<std::string, SettingValue, std::less<>>;
}
