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
}
