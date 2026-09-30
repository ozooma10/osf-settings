#include "Settings/SettingsJson.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

// Only this fixture's authored keyboard key needs the engine's name resolver.
// Keep all schema/value validation in the production parser.
namespace OSFSettings
{
    std::optional<std::uint32_t> KeyCodeFromName(std::string_view name)
    {
        if (name == "F8") return 0x77;
        throw std::runtime_error("Unexpected engine key lookup in acceptance asset check");
    }
    bool IsBindableKey(std::uint32_t code) { return code == 0x77; }
}

int main()
{
    try {
        std::ifstream file("examples/acceptance/data/SFSE/Plugins/OSF/Settings/schemas/osfsettings-test.json");
        std::string error;
        auto schema = OSFSettings::SettingsJson::ParseSchema(file,"osfsettings-test",error);
        if (!schema) throw std::runtime_error(error);
        std::size_t settings{}, actions{};
        for (const auto& group : schema->groups) {
            for (const auto& control : group.controls) {
                if (std::holds_alternative<OSFSettings::SettingDefinition>(control)) ++settings;
                else ++actions;
            }
        }
        std::cout << "Production schema parser accepted " << settings << " settings, " << actions
                  << " actions, " << schema->hotkeys.size() << " hotkeys, " << schema->menus.size() << " native launchers.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Acceptance asset validation failed: " << error.what() << '\n';
        return 1;
    }
}
