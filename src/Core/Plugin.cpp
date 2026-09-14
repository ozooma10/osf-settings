#include "Plugin.h"
#include "Runtime.h"
#include "Input/PauseMenu.h"
#include "Menu/OSFSettingsMenu.h"

#include <exception>

namespace OSFSettings::Plugin
{
    namespace
    {
        void OnMessage(SFSE::MessagingInterface::Message* message)
        {
            if(!message) { return; }

            if(message->type == SFSE::MessagingInterface::kPostPostDataLoad) {
                const bool available = OSFSettingsMenu::Register() && PauseMenu::Install();
                REX::INFO("[kPostPostDataLoad] Slim menu integration available={}", available);
            } 
     
        }
    }

    bool OnLoad()
    {
        if (!Runtime::Get().Initialize()) return false;
        const auto* messaging = SFSE::GetMessagingInterface();
        return messaging && messaging->RegisterListener(OnMessage);
    }
}
