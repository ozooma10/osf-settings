#pragma once

#include "RE/G/GameMenuBase.h"

namespace OSFSettings
{
    class OSFSettingsMenu final : public RE::GameMenuBase
    {
    public:
        SF_MENU_NAME("OSFSettingsMenu");
        OSFSettingsMenu();
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

        static bool Register();
        static void Open();

    private:
        void Close();
        void OnStartupFailed(std::string_view message);
        static RE::Scaleform::Ptr<RE::IMenu> Create();
    };
}
