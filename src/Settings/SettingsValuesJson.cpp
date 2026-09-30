#include "SettingsJson.h"
#include "Persistence/AtomicFile.h"

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace OSFSettings::SettingsJson
{
    SettingValues LoadValues(const std::filesystem::path& path, const ModSchema& schema, std::vector<SettingsLoadError>& errors)
    {
        SettingValues values;
        try {
            // No saved file is normal on the first launch. All values inherit defaults.
            std::error_code error;
            const bool exists = std::filesystem::exists(path, error);
            if (error) throw std::runtime_error(error.message());
            if (!exists) return values;

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
                const auto* setting = schema.FindSetting(key);
                if (!setting) {
                    continue; // Removed or unknown settings are ignored.
                }
                const auto decoded = DecodeValue(value, *setting);
                if (!decoded || !IsValidValue(*setting, *decoded)) {
                    errors.push_back({ path, "saved value does not match the setting's type or validation rules: " + key });
                    continue;
                }
                // Legacy snapshots cannot reveal intent: retain every valid entry, including values equal to the current schema default.
                values.emplace(key, *decoded);
            }
        } catch (const std::exception& error) {
            errors.push_back({ path, error.what() });
        }
        return values;
    }

    nlohmann::json EncodeValues(const SettingValues& values)
    {
        auto encoded = nlohmann::json::object();
        for (const auto& [key, value] : values) {
            if (const auto* number = std::get_if<double>(&value); number && !std::isfinite(*number)) {
                throw std::runtime_error("value must be finite: " + key);
            }
            std::visit([&](const auto& current) {
                if constexpr (std::is_same_v<std::decay_t<decltype(current)>, KeyBinding>) {
                    encoded[key] = current.keyCode;
                } else if constexpr (std::is_same_v<std::decay_t<decltype(current)>, EnumValue>) {
                    encoded[key] = current.value;
                } else {
                    encoded[key] = current;
                }
            }, value);
        }
        return encoded;
    }

    bool SaveValues(const std::filesystem::path& path, const SettingValues& values, std::string& error)
    {
        try {
            const nlohmann::json document = {{"formatVersion", 1}, {"values", EncodeValues(values)}};
            return Persistence::WriteAtomic(path, document.dump(2) + '\n', error);
        } catch (const std::exception& exception) { error = path.string() + ": " + exception.what(); return false; }
    }
}
