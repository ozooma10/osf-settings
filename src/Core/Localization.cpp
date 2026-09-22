#include "Settings/Localization.h"
#include "Settings/SettingsService.h"
#include "Utils/Paths.h"
#include "RE/I/INISettingCollection.h"

#include <mutex>

namespace OSFSettings::Localization
{
    void Initialize()
    {
        static std::once_flag initialized;
        std::call_once(initialized, [] {
            auto* ini = RE::INISettingCollection::GetSingleton();
            const auto* setting = ini ? ini->GetSetting("sLanguage:General") : nullptr;
            const auto language = setting && setting->GetType() == RE::Setting::Type::kString ? setting->GetString() : std::string_view("en");
            auto& settings = SettingsService::Get();
            settings.Localize(Paths::LocalizationDir(), language);
            const auto catalog = Get();
            REX::INFO("Localization: game language={}, catalog={}", language, catalog->Language());
            for (const auto& error : catalog->Errors()) REX::WARN("Localization {}: {}", error.file.string(), error.message);
        });
    }
}
