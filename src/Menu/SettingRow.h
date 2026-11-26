#pragma once

#include "Settings/SettingsSchema.h"

#include <utility>

namespace OSFSettings
{
    // Scalars use the same types as the movie bridge; int64 settings remain strings.
    using RowValue = std::variant<bool, double, std::string>;

    struct SettingRow
    {
        std::vector<std::pair<const char*, RowValue>> fields;
        std::vector<EnumOption> options;
    };

    SettingRow MakeSettingRow(const SettingDefinition& setting, const SettingValue& value);
    // The editor reads and writes the bound setting rows, so a placement row only describes the geometry.
    SettingRow MakePlacementRow(const PlacementDefinition& placement);
}
