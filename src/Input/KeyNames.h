#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace OSFSettings
{
    // Native keyboard virtual-key codes. Escape is reserved for cancellation.
    bool IsBindableKey(std::uint32_t keyCode);
    constexpr bool IsMouseKey(std::uint32_t code) { return code == 1 || code == 2 || code == 4 || code == 5 || code == 6; }
    constexpr std::uint32_t MouseVirtualKey(std::uint32_t button)
    {
        constexpr std::uint32_t keys[]{ 1, 2, 4, 5, 6 };
        return button < 5 ? keys[button] : 0;
    }

    // Uses the baked keyboard table before device initialization; unknown names return 0xFFFFFFFF.
    std::uint32_t GetKeyboardVirtualKey(std::string_view keyName);

    std::string KeyName(std::uint32_t keyCode);
    std::optional<std::uint32_t> KeyCodeFromName(std::string_view name);
}
