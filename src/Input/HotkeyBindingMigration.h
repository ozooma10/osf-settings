#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace OSFSettings
{
    // Repairs old saved keyboard bindings for the listed hotkeys before native loading.
    // Returns a repaired copy, or nullopt if unchanged or malformed. Other bindings are preserved.
    std::optional<std::string> RepairLegacyHotkeyBindings(std::string_view block, std::span<const std::string> registeredActions);
}
