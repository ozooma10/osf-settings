#include <SFSE/SFSE.h>

#include "OSFSettings.h"

#include <atomic>

namespace
{
    OSFSettings::API::Client g_settings;
    bool g_registered{};
    std::atomic_uint g_enabled{};

    void OnHotkey(const char* mod, const char* id, void* context) noexcept
    {
        auto& enabled = *static_cast<std::atomic_uint*>(context);
        const bool next = enabled.fetch_xor(1u) == 0;
        REX::INFO("Hotkey example: {}/{} enabled={}", mod, id, next);
        // Schedule game effects in their required context; this callback runs inline during input handling.
    }

    void OnMessage(SFSE::MessagingInterface::Message* message)
    {
        if (!message || message->type != SFSE::MessagingInterface::kPostLoad || g_registered) return;
        if (!g_settings.Init()) {
            REX::WARN("Hotkey example: OSF Settings is unavailable");
            return;
        }
        const auto status = g_settings.RegisterHotkey(
            "osfsettings-hotkeys-example", "toggleFeature", OnHotkey, &g_enabled);
        g_registered = status == OSFSettings::API::Status::Ok;
        if (status != OSFSettings::API::Status::Ok) {
            REX::ERROR("Hotkey example: registration failed with status {}", static_cast<std::uint32_t>(status));
        }
        // Registration, its callback code and its owner all live until process exit.
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* sfse)
{
    SFSE::Init(sfse, { .logRotate = 1, .hook = false });
    const auto* messaging = SFSE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(OnMessage);
}
