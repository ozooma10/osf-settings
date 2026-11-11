#pragma once

#include <cstdint>

namespace OSFSettings
{
    // Returns the HRESULT of handing the Nexus URL to the desktop shell.
    std::int32_t OpenModPage(std::uint32_t nexusModId);
}
