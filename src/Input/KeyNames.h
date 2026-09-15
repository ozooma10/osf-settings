#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace OSFSettings
{
    // Native keyboard virtual-key codes. Escape is reserved for cancellation.
    bool IsBindableKey(std::uint32_t keyCode);

    std::string KeyName(std::uint32_t keyCode);
    std::optional<std::uint32_t> KeyCodeFromName(std::string_view name);
}
