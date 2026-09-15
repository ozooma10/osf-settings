#include "SettingsJson.h"

#include <fstream>
#include <stdexcept>
#include <system_error>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#undef ERROR
#endif

namespace OSFSettings::SettingsJson
{
    void LoadValues(const std::filesystem::path& path, SettingValues& values, std::vector<SettingsLoadError>& errors)
    {
        try {
            // No saved file is normal on the first launch. Keep the defaults.
            std::error_code error;
            const bool exists = std::filesystem::exists(path, error);
            if (error) throw std::runtime_error(error.message());
            if (!exists) return;

            std::ifstream input(path);
            if (!input) throw std::runtime_error("cannot open values file");
            const auto document = nlohmann::json::parse(input);
            if (!document.is_object()) throw std::runtime_error("values file must be an object");
            const auto version = document.find("formatVersion");
            if (version == document.end() || !version->is_number_integer() || *version != 1) {
                throw std::runtime_error("formatVersion must be the integer 1");
            }
            const auto saved = document.find("values");
            if (saved == document.end() || !saved->is_object()) throw std::runtime_error("values must be an object");

            for (const auto& [key, value] : saved->items()) {
                const auto current = values.find(key);
                if (current == values.end()) continue; // Removed or unknown settings are ignored.
                if (!value.is_boolean()) {
                    errors.push_back({ path, "saved value must be a boolean: " + key });
                    continue;
                }
                current->second = value.get<bool>();
            }
        } catch (const std::exception& error) {
            errors.push_back({ path, error.what() });
        }
    }

    bool SaveValues(const std::filesystem::path& path, const SettingValues& values, std::string& error)
    {
        error.clear();
        auto temporary = path;
        temporary += ".tmp";
        bool ownsTemporary = false;

        try {
            auto saved = nlohmann::json::object();
            for (const auto& [key, value] : values) {
                saved[key] = std::get<bool>(value);
            }
            const nlohmann::json document = { { "formatVersion", 1 }, { "values", saved } };
            const auto text = document.dump(2) + '\n';
            if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());

            {
                std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
                if (!output) throw std::runtime_error("cannot open temporary values file");
                ownsTemporary = true;
                output << text;
                output.close();
                if (!output) throw std::runtime_error("cannot finish writing temporary values file");
            }

            // A sibling temporary file lets the filesystem replace the old file in one step.
            if (!::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                const auto code = static_cast<int>(::GetLastError());
                throw std::runtime_error("cannot replace values file: " + std::system_category().message(code));
            }
            
            return true;
        } catch (const std::exception& exception) {
            error = path.string() + ": " + exception.what();
            if (ownsTemporary) {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
            }
            return false;
        }
    }
}
