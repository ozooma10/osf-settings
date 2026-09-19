#pragma once

#include "RE/G/GameMenuBase.h"
#include "Input/KeyCapture.h"
#include "Input/NativeBindingEditor.h"
#include "Input/BindingSnapshot.h"

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
        RE::UI_MESSAGE_RESULT ProcessMessage(RE::UIMessageData& message) override;
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
        static RE::Scaleform::Ptr<RE::IMenu> Create();
        KeyCapture m_capture;
        NativeBindingEditor m_bindingEditor;
        std::shared_ptr<BindingSnapshot> m_bindings = std::make_shared<BindingSnapshot>();
        std::shared_ptr<std::atomic_uint32_t> m_textRequests = std::make_shared<std::atomic_uint32_t>();
        std::atomic_bool m_textInputActive{};
        std::string m_captureMod;
        std::string m_captureKey;
    };
}
