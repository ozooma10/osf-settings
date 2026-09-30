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

    // Native ControlMap event name for a schema hotkey.
    inline std::string EventName(std::string_view mod, std::string_view id)
    {
        std::string event(mod);
        event += '/';
        event += id;
        return event;
    }

    bool Install(); // Called once from Plugin::OnLoad, after settings load.
    const Action* FindAction(std::string_view action);
}
