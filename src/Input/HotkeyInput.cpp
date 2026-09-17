#include "HotkeyInput.h"
#include "HotkeyInputState.h"
#include "NativeHotkeys.h"
#include "NativeBindingEditor.h"
#include "harness/TestHarness.h"
#include "RE/B/BSInputEventUserStandalone.h"
#include "RE/M/MenuControls.h"
#include "RE/U/UIMessageQueue.h"
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
        using InitializeHook = REL::THook<void(RE::MenuControls*)>;
        std::optional<InitializeHook> g_initializeHook;

        class HotkeyHandler final : public RE::BSInputEventUserStandalone
        {
        public:
            bool ShouldHandleEvent(const RE::InputEvent* event) override
            {
                if (NativeBindingEditor::IsActive() || !event || event->eventType != RE::InputEvent::EventType::kButton || event->deviceType != RE::InputEvent::DeviceType::kKeyboard) {
                    return false;
                }
                const auto* button = static_cast<const RE::ButtonEvent*>(event);
                return !button->disabled && !NativeHotkeys::GetMenu(button->QUserEvent().c_str()).empty();
            }

            void OnButtonEvent(const RE::ButtonEvent* button) override
            {
                TestHarness::ObserveInput(button, true);
                const auto menu = NativeHotkeys::GetMenu(button->QUserEvent().c_str());
                if (menu.empty() || !HotkeyInputState::Get().ProcessButton(static_cast<std::uint32_t>(button->idCode), button->QUserEvent().c_str(), button->value, button->heldDownSecs)) {
                    return;
                }
                auto* queue = RE::UIMessageQueue::GetSingleton();
                if (!queue) return;
                queue->AddMessage(RE::BSFixedString(menu), RE::UI_MESSAGE_TYPE::kShow);
                //Stop further processing of this button event
                const_cast<RE::ButtonEvent*>(button)->status = RE::InputEvent::Status::kStop;
            }
        };

        // Intentionally retained to avoid engine calls during DLL teardown.
        auto& g_handler = *new std::optional<HotkeyHandler>{};

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
            TestHarness::SetInputAttached(true);
            REX::INFO("Hotkeys: MenuControls handler registered ({} -> {}, thread={})",
                before, controls->GetHandlerCount(), ::GetCurrentThreadId());
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
        if(!g_initializeHook->Init() || !g_initializeHook->Enable()) {
            g_initializeHook.reset();
            return false;
        }
      
        REX::INFO("Hotkeys: native handler lifecycle hooks installed");
        return true;
    }
}
