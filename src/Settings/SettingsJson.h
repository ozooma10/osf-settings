#pragma once

#include "SettingsSchema.h"

#include <optional>
#include <nlohmann/json_fwd.hpp>

namespace OSFSettings::SettingsJson
{
    // JSON ends here: the rest of the plugin uses ModSchema and bool values.
    std::optional<ModSchema> ParseSchema(const nlohmann::json& document, std::string& error);
}
