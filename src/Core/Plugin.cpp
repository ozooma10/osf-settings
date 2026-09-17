#include "Plugin.h"
#include "Runtime.h"
#include "Input/HotkeyInput.h"
#include "Input/NativeHotkeys.h"
#include "Input/NativeBindingEditor.h"
#include "Input/PauseMenu.h"
#include "Menu/OSFSettingsMenu.h"

namespace OSFSettings::Plugin
{
    namespace
    {
        void OnMessage(SFSE::MessagingInterface::Message* message)
        {
            if(!message) { return; }

            if(message->type == SFSE::MessagingInterface::kPostPostDataLoad) {
                const bool available = OSFSettingsMenu::Register() && PauseMenu::RegisterSink();
                REX::INFO("[kPostPostDataLoad] Settings menu integration available={}", available);
            } 
     
        }
    }

    bool OnLoad()
    {
        const auto* messaging = SFSE::GetMessagingInterface();
        if (!messaging || !Runtime::Get().Initialize() || !messaging->RegisterListener(OnMessage)) return false;
        if (!PauseMenu::Install()) REX::ERROR("Pause menu hook is unavailable");
        if (!NativeHotkeys::Install()) REX::ERROR("Native hotkey registration is unavailable");
        if (!NativeBindingEditor::Install()) REX::ERROR("Native binding editor is unavailable");
        if (!HotkeyInput::Install()) REX::ERROR("Native hotkey input hook is unavailable");
        return true;
    }
}
