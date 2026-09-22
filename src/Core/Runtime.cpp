#include "Runtime.h"
#include "SettingsDispatcher.h"
#include "Actions/ActionService.h"
#include "Launcher/LauncherService.h"
#include "Utils/Paths.h"

namespace OSFSettings
{
    Runtime &Runtime::Get()
    {
        static Runtime instance;
        return instance;
    }

    bool Runtime::Initialize() noexcept
    {
        if (m_initialized) return true;

        if (!Paths::Initialize()) return false;
        const auto schemaDir = Paths::SchemasDir();
        REX::INFO("Loading schemas from {}", schemaDir.string());
        auto& settings = SettingsService::Get();
        settings.Load(schemaDir, Paths::ValuesDir());
        const auto errors = settings.LoadErrors();
        const auto mods = settings.Snapshot();

        for (const auto& error : errors) {
            REX::ERROR("Settings {}: {}", error.file.string(), error.message);
        }

        std::size_t settingCount = 0;
        for (const auto& mod : mods) {
            for (const auto& [key, value] : mod.values) {
                std::visit([&](const auto& current) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(current)>, KeyBinding>) {
                        REX::INFO("Loaded {} / {} = {} (current value)", mod.schema.id, key, current.keyCode);
                    } else if constexpr (std::is_same_v<std::decay_t<decltype(current)>, EnumValue>) {
                        REX::INFO("Loaded {} / {} = {} (current value)", mod.schema.id, key, current.value);
                    } else {
                        REX::INFO("Loaded {} / {} = {} (current value)", mod.schema.id, key, current);
                    }
                }, value);
                ++settingCount;
            }
        }
        REX::INFO("OSF Settings Loaded: {} mod(s), {} setting(s), {} load error(s)", mods.size(), settingCount, errors.size());
        if (!SettingsDispatcher::Install()) {
            REX::ERROR("Settings notification dispatcher is unavailable");
            return false;
        }
        ActionService::Get().Initialize(mods);
        LauncherService::Get().Initialize(mods);
        settings.Start();
        m_initialized = true;
        return true;
    }

    SettingsError Runtime::SetValue(std::string_view mod, std::string_view key, SettingValue value)
    {
        const auto result = SettingsService::Get().SetValue(mod, key, value);
        if (result == SettingsError::None) {
            std::visit([&](const auto& current) {
                if constexpr (std::is_same_v<std::decay_t<decltype(current)>, KeyBinding>) {
                    REX::INFO("OSF Settings saved {} / {} = {}", mod, key, current.keyCode);
                } else if constexpr (std::is_same_v<std::decay_t<decltype(current)>, EnumValue>) {
                    REX::INFO("OSF Settings saved {} / {} = {}", mod, key, current.value);
                } else {
                    REX::INFO("OSF Settings saved {} / {} = {}", mod, key, current);
                }
            }, value);
        } else {
            REX::ERROR("OSF Settings could not save {} / {}: status {}", mod, key, static_cast<std::uint32_t>(result));
        }
        return result;
    }
}
