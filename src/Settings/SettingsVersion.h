#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace OSFSettings
{
    // Release version, independent of the JSON schema and native API versions.
    struct SettingsVersion
    {
        std::array<std::uint16_t, 3> parts{};
        auto operator<=>(const SettingsVersion&) const = default;
        std::string String() const;
        static std::optional<SettingsVersion> Parse(std::string_view text);
    };
}
