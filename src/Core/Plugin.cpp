#include "Plugin.h"

namespace OSFSettings::Plugin
{
    namespace
    {
        void OnMessage(SFSE::MessagingInterface::Message* a_message)
		{
			if (!a_message) return;
		}
    }
    
    bool OnLoad()
    {
        if (const auto* messaging = SFSE::GetMessagingInterface()) {
            messaging->RegisterListener(OnMessage);
        }
        return true;
    }
}