#pragma once

#include "Settings/SettingsError.h"
#include "NativeHotkeys.h"

#include <cstdint>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace OSFSettings
{
    class HotkeyInputState
    {
    public:
        using Block = std::uint64_t;
        using Callback = void (*)(const char* mod, const char* id, void* context) noexcept;
        enum class Target { Callback, Menu, Invalid };
        using Declarations = std::map<std::string, std::map<std::string, Target, std::less<>>, std::less<>>;

        static HotkeyInputState& Get();
        void Initialize(Declarations declarations);
        SettingsError Register(std::string_view mod, std::string_view id, Callback callback, void* context);

        using Subscription = std::uint64_t;
        SettingsError Subscribe(std::string_view mod, std::string_view id, std::function<void()> callback, Subscription& out);
        bool Unsubscribe(Subscription subscription);

        Block AcquireBlock();
        bool ReleaseBlock(Block block);
        bool Blocked();

        // The native handler resolves the action before passing it here.
        bool ProcessButton(std::uint32_t key, const NativeHotkeys::Action& action, float value, float heldSeconds,
            std::uint32_t device = 0, std::uint32_t instance = 0);
        void ResetHeldButtons();

    private:
        struct Listener
        {
            Callback callback{};
            void* context{};
        };

        struct Observer
        {
            std::string action;
            std::function<void()> callback;
            std::atomic_bool active{ true };
        };
        SettingsError Validate(std::string_view mod, std::string_view id) const;

        std::mutex m_mutex;
        Declarations m_declarations;
        std::map<std::string, std::vector<Listener>, std::less<>> m_callbacks;
        std::map<Subscription, std::shared_ptr<Observer>> m_observers;
        Subscription m_nextSubscription{ 1 };
        bool m_initialized{};
        std::set<Block> m_blocks;
        Block m_nextBlock{ 1 };
        // Native device type, device instance, and device-local button code.
        std::map<std::array<std::uint32_t, 3>, std::string> m_pressed;
    };
}
