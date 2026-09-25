#include <SFSE/SFSE.h>
#include "OSFSettings.h"

namespace
{
    OSFSettings::API::Client g_actions;

    void OnAction(std::uint64_t invocation, const char* mod, const char* id, void* context) noexcept
    {
        auto& actions = *static_cast<OSFSettings::API::Client*>(context);
        REX::INFO("Action example: {}/{} invocation={}", mod, id, invocation);
        // Short work can finish here. For longer work, call CompleteAction when finished, even after menu close.
        const auto result = actions.CompleteAction(invocation, true, "The native action handler ran.");
        if (result != OSFSettings::API::Status::Ok) {
            REX::WARN("Action example completion status={}", static_cast<std::uint32_t>(result));
        }
    }
    void OnMessage(SFSE::MessagingInterface::Message* message)
    {
        if (!message || message->type != SFSE::MessagingInterface::kPostLoad) return;
        if (!g_actions.Init()) { REX::WARN("Action example: provider unavailable"); return; }
        const auto result = g_actions.RegisterAction("osfsettings-actions-example", "run", OnAction, &g_actions);
        REX::INFO("Action example registration status={}", static_cast<std::uint32_t>(result));
    }
}
SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* sfse)
{
    SFSE::Init(sfse, { .logRotate = 1, .hook = false });
    const auto* messaging = SFSE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(OnMessage);
}
