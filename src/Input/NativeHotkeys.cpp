#include "NativeHotkeys.h"
#include "Settings/SettingsService.h"
#include "RE/C/ControlMap.h"
#include "RE/U/UserEvents.h"
#include "SFSE/InputMap.h"
#include "REL/THook.h"

#include <cstddef>
#include <optional>

namespace OSFSettings::NativeHotkeys
{
    namespace
    {
        constexpr std::ptrdiff_t kParseMappingsCallOffset = 0x45; // Parser call inside LoadMappings.
        constexpr std::uint32_t kUnbound = 0xFFu;
        constexpr std::uint32_t kUnknownKey = 0xFFFFFFFFu;

        using ParseHook = REL::THook<void(RE::ControlMap*, const char*)>;
        std::optional<ParseHook> g_parseHook;
        std::string g_hotkeyDefinitions;

        void HookedParseMappings(RE::ControlMap* map, const char* vanilla)
        {
            const auto definitions = g_hotkeyDefinitions + vanilla;
            (*g_parseHook)(map, definitions.c_str());
        }
    }

    bool Install()
    {
        if (g_parseHook) {
            return g_parseHook->GetEnabled();
        }

        //Need to build hotkey definitions that will get pre-pended to vanilla binding map
        g_hotkeyDefinitions.clear();
        for (const auto& mod : SettingsService::Get().Snapshot()) {
            for (const auto& hotkey : mod.schema.hotkeys) {
                const auto event = mod.schema.id + "/" + hotkey.id;
                const auto key = hotkey.defaultKey ? SFSE::InputMap::GetKeyboardVirtualKey(*hotkey.defaultKey) : kUnbound;
                if (key == kUnknownKey) {
                    REX::ERROR("Hotkeys: unknown default key '{}' for {}", *hotkey.defaultKey, event);
                    continue;
                }
                g_hotkeyDefinitions += RE::ControlMap::FormatMappingRow(
                    event.c_str(), key, kUnbound, kUnbound,
                    true, false, false, static_cast<std::uint32_t>(RE::USER_EVENT_FLAG::Movement), 0u, false);
            }
        }
        if (g_hotkeyDefinitions.empty()) return true;

        g_parseHook.emplace("Hotkeys::ParseMappings", RE::ID::ControlMap::LoadMappings, kParseMappingsCallOffset, &HookedParseMappings);
        if (!g_parseHook->Init() || !g_parseHook->Enable()) {
            g_parseHook.reset();
            return false;
        }
        REX::INFO("Hotkeys: native defaults hook installed");
        return true;
    }
}
