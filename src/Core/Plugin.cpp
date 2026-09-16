#include "Plugin.h"
#include "Runtime.h"
#include "Input/HotkeyService.h"
#include "Input/NativeHotkeys.h"
#include "Input/PauseMenu.h"
#include "Menu/OSFSettingsMenu.h"

namespace OSFSettings::Plugin
{
    namespace
    {
        void OnMessage(SFSE::MessagingInterface::Message* message)
        {
            if(!message) { return; }

            if(message->type == SFSE::MessagingInterface::kPostDataLoad) {
                NativeHotkeys::Start();
                static HotkeyService::Subscription openMenu{};
                if (!openMenu) {
                    const auto result = HotkeyService::Get().Subscribe("osfsettings", "openMenu", [](const auto&, const auto&) {
                        REX::INFO("Hotkeys: open OSF Settings activation on native game-thread drain");
                        OSFSettingsMenu::Open();
                    }, openMenu);
                    if (result != SettingsError::None) {
                        REX::WARN("OSF Settings open-menu action unavailable: {}", static_cast<int>(result));
                    }
                }
                const bool available = OSFSettingsMenu::Register() && PauseMenu::RegisterSink();
                REX::INFO("[kPostDataLoad] Settings menu integration available={}", available);
            } 
     
        }
    }

    bool OnLoad()
    {
        const auto* messaging = SFSE::GetMessagingInterface();
        if (!messaging || !Runtime::Get().Initialize() || !messaging->RegisterListener(OnMessage)) return false;
        if (!NativeHotkeys::Install()) REX::ERROR("Native hotkey integration is unavailable; actions will not activate");
        if (!PauseMenu::Install()) REX::ERROR("Pause menu hook is unavailable");
        return true;
    }
}
