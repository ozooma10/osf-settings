#pragma once

#include "RE/S/ScaleformGFxFunctionHandler.h"
#include <filesystem>

namespace RE
{
    namespace BSScript { class IVirtualMachine; }
    class ButtonEvent;
    class GameMenuBase;
    class UI;
}

namespace OSFSettings::TestHarness
{
#ifdef OSFSETTINGS_TEST_HARNESS
    void BindPapyrus(RE::BSScript::IVirtualMachine& vm);
    bool InitializeValues(const std::filesystem::path& gameDirectory, std::filesystem::path& valuesDirectory);
    void RegisterMenuObserver(RE::UI& ui, bool registered);
    void RegisterMenuFunctions(RE::GameMenuBase& menu);
    bool HandleMenuCall(const RE::Scaleform::GFx::FunctionHandler::Params& params) noexcept;
    // Poll queues observations to the verified game-thread drain; it never reads
    // borrowed engine state on the SFSE dispatcher thread.
    void Poll() noexcept;
    void SetInputAttached(bool attached) noexcept;
    void MenuState(bool open) noexcept;
    void ObserveInput(const RE::ButtonEvent* event, bool hotkey) noexcept;
    // Only called synchronously by the movie's code-object callback on its UI lane.
    void ObserveUI(std::uint64_t frame, const RE::Scaleform::GFx::Value* state) noexcept;
#else
    inline void BindPapyrus(RE::BSScript::IVirtualMachine&) {}
    inline bool InitializeValues(const std::filesystem::path&, std::filesystem::path&) { return true; }
    inline void RegisterMenuObserver(RE::UI&, bool) {}
    inline void RegisterMenuFunctions(RE::GameMenuBase&) {}
    inline bool HandleMenuCall(const RE::Scaleform::GFx::FunctionHandler::Params&) noexcept { return false; }
    inline void Poll() noexcept {}
    inline void SetInputAttached(bool) noexcept {}
    inline void MenuState(bool) noexcept {}
    inline void ObserveInput(const RE::ButtonEvent*, bool) noexcept {}
#endif
}
