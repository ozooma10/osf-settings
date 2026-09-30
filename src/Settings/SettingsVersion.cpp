#include "SettingsVersion.h"

#include <charconv>

namespace OSFSettings
{
    std::string SettingsVersion::String() const
    {
        return std::to_string(parts[0]) + "." + std::to_string(parts[1]) + "." + std::to_string(parts[2]);
    }

    std::optional<SettingsVersion> SettingsVersion::Parse(std::string_view text)
    {
        SettingsVersion version;
        for (std::size_t index = 0; index < version.parts.size(); ++index) {
            const auto dot = text.find('.');
            if ((index == version.parts.size() - 1) != (dot == std::string_view::npos)) return std::nullopt;
            const auto component = text.substr(0, dot);
            if (component.empty() || component.size() > 5 ||
                (component.size() > 1 && component.front() == '0')) return std::nullopt;
            const auto [end, error] = std::from_chars(component.data(), component.data() + component.size(), version.parts[index]);
            if (error != std::errc{} || end != component.data() + component.size()) return std::nullopt;
            if (dot != std::string_view::npos) text.remove_prefix(dot + 1);
        }
        return version;
    }
}
