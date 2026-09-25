#include <SFSE/SFSE.h>

#include "RegistryConsumer.h"

namespace
{
    void Describe(const OSFSettings::API::RegistryView& registry, void*) noexcept
    {
        for (std::uint32_t m = 0; m < registry.modCount; ++m) {
            const auto& mod = registry.mods[m];
            REX::INFO("Registry example: mod {} ({})", std::string_view(mod.id.data, mod.id.size),
                std::string_view(mod.title.data, mod.title.size));
            for (std::uint32_t g = 0; g < mod.groupCount; ++g) {
                const auto& group = mod.groups[g];
                for (std::uint32_t s = 0; s < group.settingCount; ++s) {
                    const auto& setting = group.settings[s];
                    REX::INFO("Registry example: group {} / key {} type={} restart={}",
                        std::string_view(group.id.data, group.id.size), std::string_view(setting.key.data, setting.key.size),
                        static_cast<std::uint32_t>(setting.type), setting.requiresRestart);
                }
            }
        }
    }

    void OnMessage(SFSE::MessagingInterface::Message* message)
    {
        static RegistryExample::RegistryConsumer* consumer{};
        if (!message || message->type != SFSE::MessagingInterface::kPostLoad || consumer) return;
        OSFSettings::API::Client settings;
        if (!settings.Init()) {
            REX::WARN("Registry example: OSF Settings is unavailable");
            return;
        }
        // Process lifetime after success; avoid engine/provider calls at DLL teardown.
        consumer = new RegistryExample::RegistryConsumer(settings);
        const auto status = consumer->Start();
        if (status != OSFSettings::API::Status::Ok) {
            REX::ERROR("Registry example: startup failed with status {}", static_cast<std::uint32_t>(status));
            delete consumer; // Stop cleans up any subscriptions before destroying the owner.
            consumer = nullptr;
            return;
        }
        settings.ReadRegistry(nullptr, Describe, nullptr);
        // consumer->Snapshot() returns owned typed values. Notifications refresh
        // them without touching engine/UI objects. A shorter-lived owner calls
        // Stop before destruction; it must not destroy itself in a callback.
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* sfse)
{
    SFSE::Init(sfse, { .logRotate = 1, .hook = false });
    const auto* messaging = SFSE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(OnMessage);
}
