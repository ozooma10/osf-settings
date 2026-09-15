#include "SettingsJson.h"

#include <limits>
#include <nlohmann/json.hpp>

namespace OSFSettings::SettingsJson
{
    std::optional<SettingValue> DecodeValue(const nlohmann::json& value)
    {
        if (value.is_boolean()) {
            return value.get<bool>();
        }
        if (value.is_number_unsigned()) {
            const auto integer = value.get<std::uint64_t>();
            if (integer > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return std::nullopt;
            return static_cast<std::int64_t>(integer);
        }
        if (value.is_number_integer()) {
            return value.get<std::int64_t>();
        }
        return std::nullopt;
    }
}
