#include "FloatSlider.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace OSFSettings
{
    std::optional<FloatSlider> MakeFloatSlider(const FloatDefinition& definition)
    {
        if (!definition.minimum || !definition.maximum || !std::isfinite(*definition.minimum) ||
            !std::isfinite(*definition.maximum) || *definition.minimum >= *definition.maximum ||
            !std::isfinite(definition.step) || definition.step <= 0.0) {
            return std::nullopt;
        }

        const auto magnitude = std::max(std::abs(*definition.minimum), std::abs(*definition.maximum));
        const auto resolution = std::nextafter(magnitude, std::numeric_limits<double>::infinity()) - magnitude;
        if (definition.step < resolution) return std::nullopt;

        std::int64_t scale = 1;
        for (int decimals = 0; decimals <= 9; decimals++, scale *= 10) {
            const auto scaled = [scale](double value) -> std::optional<std::int64_t> {
                const double integer = std::round(value * static_cast<double>(scale));
                if (!std::isfinite(integer) || std::abs(integer) > static_cast<double>(kMaxSafeInteger) || integer / static_cast<double>(scale) != value){
                    return std::nullopt;
                }
                return static_cast<std::int64_t>(integer);
            };
            const auto minimum = scaled(*definition.minimum);
            const auto maximum = scaled(*definition.maximum);
            const auto step = scaled(definition.step);
            if (!minimum || !maximum || !step || *step <= 0) continue;
            const auto span = *maximum - *minimum;
            if (span > kMaxSafeInteger) return std::nullopt;
            const auto steps = (span - 1) / *step + 1; // Include a shorter final step to max.
            if (steps > std::numeric_limits<std::uint32_t>::max()) return std::nullopt;
            return FloatSlider{ *minimum, *maximum, *step, scale, static_cast<std::uint32_t>(steps), decimals };
        }
        return std::nullopt;
    }
}
