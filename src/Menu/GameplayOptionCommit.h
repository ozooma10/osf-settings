#pragma once

#include <cstdint>

namespace OSFSettings::GameplayOptionCommit
{
    // Call only from the menu's native UI callback, with an engine-published choice index.
    // Completes the native edit and save dispatch; Papyrus listeners run independently.
    [[nodiscard]] bool Commit(std::uint32_t id, std::uint32_t type, std::uint32_t value);
}
