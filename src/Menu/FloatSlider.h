#pragma once

#include "Settings/SettingsSchema.h"

namespace OSFSettings
{
    struct FloatSlider
    {
        std::int64_t minimum{};
        std::int64_t maximum{};
        std::int64_t step{};
        std::int64_t scale{ 1 };
        std::uint32_t steps{};
        int decimals{};
    };

    std::optional<FloatSlider> MakeFloatSlider(const FloatDefinition& definition);
}
