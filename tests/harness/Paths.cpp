#include "TestHarness.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace OSFSettings::TestHarness
{
    bool InitializeValues(const std::filesystem::path& gameDirectory, std::filesystem::path& valuesDirectory)
    {
        // MO2 overwrite can win over every test mod. Keep mutable test values
        // outside its virtual Data tree, with no fallback to the normal store.
        valuesDirectory.clear();
        try {
            const auto configPath = gameDirectory / "Data" / "SFSE" / "Plugins" / "OSFSettingsTestHarness.json";
            std::ifstream input(configPath);
            if (!input) throw std::runtime_error("test harness values configuration is missing");
            const auto config = nlohmann::json::parse(input);
            const auto path = config.at("valuesDir").get<std::string>();
            if (path.empty() || path.find('\0') != std::string::npos)
                throw std::runtime_error("valuesDir must be a nonempty absolute path");
            const auto directory = std::filesystem::path(path).lexically_normal();
            if (!directory.is_absolute()) throw std::runtime_error("valuesDir must be absolute");
            valuesDirectory = directory;
            REX::INFO("Test harness isolated values: {}", valuesDirectory.string());
        } catch (const std::exception& error) {
            REX::ERROR("Test harness cannot initialize isolated values: {}", error.what());
            return false;
        }
        return true;
    }
}
