#pragma once

#include <chrono>
#include "RE/G/GameMenuBase.h"
#include "Input/KeyCapture.h"
#include "Input/NativeBindingEditor.h"
#include "Input/BindingSnapshot.h"
#include "Launcher/LauncherService.h"

namespace OSFSettings
{
    class OSFSettingsMenu final : public RE::GameMenuBase
    {
    public:
        SF_MENU_NAME("OSFSettingsMenu");
        OSFSettingsMenu();
        ~OSFSettingsMenu() override;
        const char* GetName() const override { return MENU_NAME.data(); }
        const char* GetRootPath() const override { return "root1"; }
        ScaleModeType GetViewScaleMode() override { return ScaleModeType::kShowAll; }
        RE::BSEventNotifyControl ProcessEvent(const RE::UpdateSceneRectEvent&, RE::BSTEventSource<RE::UpdateSceneRectEvent>*) override
        {
            return RE::BSEventNotifyControl::kContinue;
        }
        void PostCreate() override;
        bool UseEventDispatcher() override { return true; }
        void MapCodeObjectFunctions() override;
        void Call(const RE::Scaleform::GFx::FunctionHandler::Params& params) noexcept override;
        bool ShouldHandleEvent(const RE::InputEvent* event) override;
        bool WantsMovieEventForward(const RE::InputEvent* event) override;
        void OnButtonEvent(const RE::ButtonEvent* event) override;
        void OnRemovedFromMenuStack() override;

        static bool Register();
        static void Open();

    private:
        void Close();
        bool RequestTextInput(bool enabled);
        void OnStartupFailed(std::string_view message);
        // Native menu handoff: commit closes Pause and Settings; the destination opens once Settings has left the stack.
        void CommitLaunch(LaunchDestination destination);
        void CloseWithPause();
        static RE::Scaleform::Ptr<RE::IMenu> Create();
        std::optional<LaunchDestination> m_launch;
        // This menu instance observes only the request it started. Leaving does not cancel it.
        std::uint64_t m_waiting{};
        std::chrono::steady_clock::time_point m_waitDeadline;
        KeyCapture m_capture;
        NativeBindingEditor m_bindingEditor;
        std::shared_ptr<BindingSnapshot> m_bindings = std::make_shared<BindingSnapshot>();
        std::shared_ptr<std::atomic_uint32_t> m_textRequests = std::make_shared<std::atomic_uint32_t>();
        std::atomic_bool m_textInputActive{};
        std::string m_captureMod;
        std::string m_captureKey;
    };
}
