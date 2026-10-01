#pragma once

#include <nlohmann/json.hpp>

namespace OSFSettings::TestHarness
{
    nlohmann::json AcceptanceSnapshot();
    void RegisterAcceptanceEvents();
    void ProviderCommand(const nlohmann::json& args);
    nlohmann::json ProviderSnapshot();
}
