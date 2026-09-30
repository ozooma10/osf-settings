#include "Runtime.h"
#include "SettingsDispatcher.h"
#include "Actions/ActionService.h"
#include "Diagnostics/DiagnosticsService.h"
#include "Launcher/LauncherService.h"
#include "Utils/Paths.h"

namespace OSFSettings
{
    namespace
    {
        std::string LogText(const SettingValue& value)
        {
            return std::visit([](const auto& current) -> std::string {
                using T = std::decay_t<decltype(current)>;
                if constexpr (std::is_same_v<T, KeyBinding>) return std::to_string(current.keyCode);
                else if constexpr (std::is_same_v<T, EnumValue>) return current.value;
                else return std::format("{}", current);
            }, value);
        }
    }

    Runtime &Runtime::Get()
    {
        static Runtime instance;
        return instance;
    }

    bool Runtime::Initialize() noexcept
    {
        if (!Paths::Initialize()) return false;
        const auto schemaDir = Paths::SchemasDir();
        REX::INFO("Loading schemas from {}", schemaDir.string());
        auto& settings = SettingsService::Get();
        settings.Load(schemaDir, Paths::UserDataDir());
        const auto errors = settings.LoadErrors();
        const auto mods = settings.Snapshot();

        for (const auto& error : errors) {
            REX::ERROR("Settings {}: {}", error.file.string(), error.message);
        }
        // English until the game's language loads; Localization::Initialize reports again.
        ReportLoadIssues();

        std::size_t settingCount = 0;
        for (const auto& mod : mods) {
            for (const auto& [key, value] : mod.values) {
                REX::INFO("Loaded {} / {} = {} (current value)", mod.schema.id, key, LogText(value));
                settingCount++;
            }
        }
        REX::INFO("OSF Settings Loaded: {} mod(s), {} setting(s), {} load error(s)", mods.size(), settingCount, errors.size());
        if (!SettingsDispatcher::Install()) {
            REX::ERROR("Settings notification dispatcher is unavailable");
            return false;
        }
        ActionService::Get().Initialize(mods);
        LauncherService::Get().Initialize(mods);
        LauncherService::Get().LoadHistory(Paths::UserDataDir());
        if (!settings.Start()) {
            REX::ERROR("Settings service cannot start before its schemas are loaded");
            return false;
        }
        return true;
    }

    void Runtime::ReportLoadIssues()
    {
        for (auto& issue : SchemaLoadIssues(SettingsService::Get().LoadErrors())) {
            if (!DiagnosticsService::Get().Report(std::move(issue))) {
                REX::ERROR("Could not report a schema load failure in Mod Issues");
            }
        }
    }

    SettingsError Runtime::SetValue(std::string_view mod, std::string_view key, SettingValue value)
    {
        const auto result = SettingsService::Get().SetValue(mod, key, value);
        if (result == SettingsError::None) {
            REX::INFO("OSF Settings saved {} / {} = {}", mod, key, LogText(value));
        } else {
            REX::ERROR("OSF Settings could not save {} / {}: status {}", mod, key, static_cast<std::uint32_t>(result));
        }
        return result;
    }
}
