#pragma once

#include <nlohmann/json.hpp>

namespace OSFSettings::TestHarness
{
    nlohmann::json AcceptanceSnapshot();
    void RegisterAcceptanceEvents();
}
