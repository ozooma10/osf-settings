#include "TestHarness.h"
#include "Acceptance.h"
#include "Menu/OSFSettingsMenu.h"
#include "RE/E/Events.h"
#include "RE/U/UI.h"
#include <limits>

namespace OSFSettings::TestHarness
{
    namespace
    {
        // Keep the test callback separate from the production menu's function IDs.
        constexpr auto kSnapshotFunction = std::numeric_limits<std::uintptr_t>::max();

        class MenuObserver final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (std::string_view(event.menuName.c_str()) == OSFSettingsMenu::MENU_NAME) {
                    // The engine also emits an opening event after refused admission.
                    auto* ui = RE::UI::GetSingleton();
                    MenuState(event.opening && ui && ui->IsMenuOpen(event.menuName));
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    void RegisterMenuObserver(RE::UI& ui, bool registered)
    {
        static bool observing{};
        if (registered && !observing) {
            ui.RegisterSink(new MenuObserver);
            RegisterAcceptanceEvents();
            observing = true;
        }
    }

    void RegisterMenuFunctions(RE::GameMenuBase& menu)
    {
        menu.RegisterNativeFunction("testSnapshot", kSnapshotFunction);
    }

    bool HandleMenuCall(const RE::Scaleform::GFx::FunctionHandler::Params& params) noexcept
    {
        if (reinterpret_cast<std::uintptr_t>(params.userData) != kSnapshotFunction) return false;
        if (params.argCount && (params.args[0].IsNumber() || params.args[0].IsInt() || params.args[0].IsUInt())) {
            const auto frame = params.args[0].IsInt() ? params.args[0].GetInt() :
                params.args[0].IsUInt() ? params.args[0].GetUInt() : params.args[0].GetNumber();
            ObserveUI(static_cast<std::uint64_t>(frame), params.argCount > 1 ? &params.args[1] : nullptr);
        }
        return true;
    }
}
