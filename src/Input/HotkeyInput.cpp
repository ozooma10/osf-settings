#include "HotkeyInput.h"
#include "Menu/SettingsMenuActivation.h"
#include "RE/B/BSInputEventUserStandalone.h"
#include "RE/M/MenuControls.h"
#include "REL/THook.h"

#include <Windows.h>
#undef ERROR
#include <cstddef>
#include <optional>
#include <string_view>

namespace OSFSettings::HotkeyInput
{
    namespace
    {
        constexpr std::ptrdiff_t kInitializeCall = 0x303;

        using LifecycleHook = REL::THook<void(RE::MenuControls*)>;
        std::optional<LifecycleHook> g_initializeHook;
        RE::MenuControls* g_attachedControls{}; // game input dispatcher our handler attached to.

        //Hook into game input events to forward registered hotkeys to requesting mods
        class HotkeyHandler final : public RE::BSInputEventUserStandalone
        {
        public:
            bool ShouldHandleEvent(const RE::InputEvent* event) override
            {
                if (!event || event->eventType != RE::InputEvent::EventType::kButton || event->deviceType != RE::InputEvent::DeviceType::kKeyboard) {
                    return false;
                }
                const auto* button = static_cast<const RE::ButtonEvent*>(event);
                return !button->disabled && std::string_view{ button->QUserEvent().c_str() } == "osfsettings/openMenu";
            }

            void OnButtonEvent(const RE::ButtonEvent* button) override
            {
                SettingsMenuActivation::OnHotkeyEvent(*button);
            }
        };

        std::optional<HotkeyHandler> g_handler;

        void Attach(RE::MenuControls* controls)
        {
            if (g_handler) {
                REX::ERROR("Hotkeys: duplicate MenuControls initialization; handler not added again");
                return;
            }
            auto& handler = g_handler.emplace();
            const auto before = controls->GetHandlerCount();
            if (!controls->RegisterHandler(&handler)) {
                g_handler.reset();
                REX::ERROR("Hotkeys: could not register native handler; input disabled");
                return;
            }
            g_attachedControls = controls;
            SettingsMenuActivation::SetInputAttached(true);
            REX::INFO("Hotkeys: MenuControls handler registered ({} -> {}, thread={})",
                before, controls->GetHandlerCount(), ::GetCurrentThreadId());
        }

        void Detach(RE::MenuControls* controls)
        {
            if (controls != g_attachedControls || !g_handler) return;
            SettingsMenuActivation::SetInputAttached(false);
            g_handler->inputEventHandlingEnabled = false;
            controls->UnregisterHandler(&*g_handler);
            g_handler.reset();
            g_attachedControls = nullptr;
            REX::INFO("Hotkeys: MenuControls handler removed (remaining={})", controls->GetHandlerCount());
        }

        void InitializeHandlers(RE::MenuControls* controls)
        {
            (*g_initializeHook)(controls);
            Attach(controls);
        }
    }

    bool Install()
    {
        if (g_initializeHook) {
            return g_initializeHook->GetEnabled();
        }
        if (RE::MenuControls::GetSingleton()) {
            REX::ERROR("Hotkeys: MenuControls already exists; safe startup registration was missed");
            return false;
        }

        g_initializeHook.emplace("Hotkeys::Initialize", RE::ID::Main::InitializeInputSingletons, kInitializeCall, &InitializeHandlers);
        for (auto* hook : { &g_shutdownHook, &g_destroyHook, &g_initializeHook }) {
            if (!(*hook)->Init() || !(*hook)->Enable()) {
                g_initializeHook.reset();
                return false;
            }
        }
        REX::INFO("Hotkeys: native handler lifecycle hooks installed");
        return true;
    }

    bool RegisterMenuEvents()
    {
        return g_initializeHook && g_initializeHook->GetEnabled() && SettingsMenuActivation::RegisterMenuEvents();
    }
}
