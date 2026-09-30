#pragma once

#include "../../sdk/OSFSettings.h"

namespace OSFSettings::API
{
    // Shared by every service export: serve the same major at an equal or newer minor.
    inline void* Negotiate(std::uint32_t served, std::uint32_t requested, std::uint32_t* outVersion, void* api) noexcept
    {
        if (outVersion) *outVersion = 0;
        if (!Supports(served, requested)) return nullptr;
        if (outVersion) *outVersion = served;
        return api;
    }
}
