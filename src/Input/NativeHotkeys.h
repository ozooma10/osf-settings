#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace OSFSettings::NativeHotkeys
{
    struct Action
    {
        std::string event;
        std::string mod;
        std::string id;
        std::optional<std::string> menu;
    };

    bool Install();
    const Action* FindAction(std::string_view action);
}
