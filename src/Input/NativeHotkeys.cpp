#include "NativeHotkeys.h"
#include "HotkeyInputState.h"
#include "Settings/SettingsService.h"
#include "RE/C/ControlMap.h"
#include "RE/U/UserEvents.h"
#include "KeyNames.h"
#include "HotkeyBindingMigration.h"
#include "REL/THook.h"

#include <cstddef>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace OSFSettings::NativeHotkeys
{
    namespace
    {
        constexpr std::ptrdiff_t kParseMappingsCallOffset = 0x45; // Parser call inside LoadMappings.
        constexpr std::uint32_t kUnbound = KeyBinding::Unbound;
        constexpr std::ptrdiff_t kApplyCustomMappingsCallOffset = 0x12A;

        using ParseHook = REL::THook<void(RE::ControlMap*, const char*)>;
        std::optional<ParseHook> g_parseHook;
        using CustomBlockHook = REL::THook<void(RE::ControlMap*, std::uint8_t, const char*)>;
        std::optional<CustomBlockHook> g_customBlockHook;
        std::string g_hotkeyDefinitions;
        std::map<std::string, Action, std::less<>> g_actions;
        std::vector<std::string> g_registeredActions;

        void HookedParseMappings(RE::ControlMap* map, const char* vanilla)
        {
            const auto definitions = g_hotkeyDefinitions + vanilla;
            (*g_parseHook)(map, definitions.c_str());
        }

        void HookedApplyCustomMappings(RE::ControlMap* map, std::uint8_t device, const char* block)
        {
            std::optional<std::string> replacement;
            if (device == 0) {
                const auto size = (static_cast<unsigned char>(block[1]) << 8) | static_cast<unsigned char>(block[2]);
                if (size >= 3 && size <= 0x4000) {
                    replacement = RepairLegacyHotkeyBindings({block, static_cast<std::size_t>(size)}, g_registeredActions);
                    if (replacement) {
                        REX::INFO("Hotkeys: migrated legacy saved bindings");
                    }
                }
            }
            // The complete device block goes through native loading exactly once; the caller still resolves links and sorts. Never rewrite the file.
            (*g_customBlockHook)(map, device, replacement ? replacement->c_str() : block);
        }
    }

    bool Install()
    {
        //Need to build hotkey definitions that will get pre-pended to vanilla binding map
        HotkeyInputState::Declarations declarations;
        for (const auto& mod : SettingsService::Get().Snapshot()) {
            // A hidden integration's hotkeys could be neither seen nor rebound.
            if (mod.schema.hidden) continue;
            auto& targets = declarations[mod.schema.id];
            for (const auto& hotkey : mod.schema.hotkeys) {
                const auto event = EventName(mod.schema.id, hotkey.id);
                auto& target = targets[hotkey.id];
                target = hotkey.menu ? HotkeyInputState::Target::Menu : HotkeyInputState::Target::Invalid;
                const auto key = hotkey.defaultKey ? GetKeyboardVirtualKey(*hotkey.defaultKey) : kUnbound;
                if (key == kUnknownKey) {
                    REX::ERROR("Hotkeys: unknown default key '{}' for {}", *hotkey.defaultKey, event);
                    continue;
                }
                // Menu actions use Pause's control mask (0x08), independent of Movement.
                const auto mask = hotkey.menu ? RE::USER_EVENT_FLAG::TabMenuMaybe : RE::USER_EVENT_FLAG::Movement;
                // Explicit primary identity stays stable even when the default is unbound.
                const auto keyboard = std::format("{:#x}*0", key);
                g_hotkeyDefinitions += RE::ControlMap::FormatMappingRow(event.c_str(), keyboard.c_str(), "0xff", "0xff",
                    true, false, true, static_cast<std::uint32_t>(mask), 0u, false);
                g_actions.emplace(event, Action{ event, mod.schema.id, hotkey.id, hotkey.menu });
                g_registeredActions.push_back(event);
                if (!hotkey.menu) {
                    target = HotkeyInputState::Target::Callback;
                }
            }
        }
        bool migrationAvailable = true;
        if (!g_hotkeyDefinitions.empty()) {
            g_parseHook.emplace("Hotkeys::ParseMappings", RE::ID::ControlMap::LoadMappings, kParseMappingsCallOffset, &HookedParseMappings);
            if (!g_parseHook->Init() || !g_parseHook->Enable()) {
                g_parseHook.reset();
                g_actions.clear();
                g_registeredActions.clear();
                return false;
            }
            REX::INFO("Hotkeys: native defaults hook installed");
            g_customBlockHook.emplace("Hotkeys::SavedBindings", RE::ID::ControlMap::LoadCustomMappings, kApplyCustomMappingsCallOffset, &HookedApplyCustomMappings);
            migrationAvailable = g_customBlockHook->Init() && g_customBlockHook->Enable();
            if (!migrationAvailable) {
                g_customBlockHook.reset();
                REX::ERROR("Hotkeys: saved binding migration is unavailable");
            }
        }
        HotkeyInputState::Get().Initialize(std::move(declarations));
        return migrationAvailable;
    }

    const Action* FindAction(std::string_view action)
    {
        const auto found = g_actions.find(action);
        return found != g_actions.end() ? &found->second : nullptr;
    }
}
