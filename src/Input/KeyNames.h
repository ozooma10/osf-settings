#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace OSFSettings
{
    // Native keyboard virtual-key codes. Escape is reserved for cancellation.
    bool IsBindableKey(std::uint32_t keyCode);

    // Virtual-key to DirectInput scan code; returns 0 if unmappable.
    std::uint32_t VirtualKeyToKeycode(std::uint32_t virtualKey);

    // Uses the baked keyboard table before device initialization; unknown names return 0xFFFFFFFF.
    std::uint32_t GetKeyboardVirtualKey(std::string_view keyName);

    std::string KeyName(std::uint32_t keyCode);
    std::optional<std::uint32_t> KeyCodeFromName(std::string_view name);
}
