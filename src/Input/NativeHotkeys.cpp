#include "NativeHotkeys.h"
#include "HotkeyInputState.h"
#include "Settings/SettingsService.h"
#include "RE/C/ControlMap.h"
#include "RE/U/UserEvents.h"
#include "SFSE/InputMap.h"
#include "SFSE/API.h"
#include "REL/THook.h"

#include <cstddef>
#include <map>
#include <optional>
#include <utility>

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
        std::map<std::string, Action, std::less<>> g_actions;

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
        g_actions.clear();
        HotkeyInputState::Declarations declarations;
        for (const auto& mod : SettingsService::Get().Snapshot()) {
            auto& targets = declarations[mod.schema.id];
            for (const auto& hotkey : mod.schema.hotkeys) {
                const auto event = mod.schema.id + "/" + hotkey.id;
                auto& target = targets[hotkey.id];
                target = hotkey.menu ? HotkeyInputState::Target::Menu : HotkeyInputState::Target::Invalid;
                const auto key = hotkey.defaultKey ? SFSE::InputMap::GetKeyboardVirtualKey(*hotkey.defaultKey) : kUnbound;
                if (key == kUnknownKey) {
                    REX::ERROR("Hotkeys: unknown default key '{}' for {}", *hotkey.defaultKey, event);
                    continue;
                }
                // Menu actions use Pause's control mask (0x08), independent of Movement.
                const auto mask = hotkey.menu ? RE::USER_EVENT_FLAG::TabMenuMaybe : RE::USER_EVENT_FLAG::Movement;
                g_hotkeyDefinitions += RE::ControlMap::FormatMappingRow(event.c_str(), key, kUnbound, kUnbound,
                    true, false, false, static_cast<std::uint32_t>(mask), 0u, false);
                g_actions.emplace(event, Action{ mod.schema.id, hotkey.id, hotkey.menu });
                if (!hotkey.menu) {
                    target = HotkeyInputState::Target::Callback;
                }
            }
        }
        if (!g_hotkeyDefinitions.empty()) {
            g_parseHook.emplace("Hotkeys::ParseMappings", RE::ID::ControlMap::LoadMappings, kParseMappingsCallOffset, &HookedParseMappings);
            if (!g_parseHook->Init() || !g_parseHook->Enable()) {
                g_parseHook.reset();
                g_actions.clear();
                return false;
            }
            REX::INFO("Hotkeys: native defaults hook installed");
        }
        HotkeyInputState::Get().Initialize(std::move(declarations));
        return true;
    }

    const Action* FindAction(std::string_view action)
    {
        const auto found = g_actions.find(action);
        return found != g_actions.end() ? &found->second : nullptr;
    }
}
