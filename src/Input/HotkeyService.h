#pragma once
#include "Settings/SettingsService.h"
#include <chrono>
#include <span>

namespace RE { class InputEvent; }

namespace OSFSettings
{
    struct HotkeyAction { std::string mod, id, eventName, label; std::uint32_t defaultKey = 0xFF; };

    class HotkeyService
    {
    public:
        using Subscription = std::uint64_t;
        using HotkeyBlock = std::uint64_t;
        using Callback = std::function<void(const std::string& mod, const std::string& action)>;
        using Clock = std::chrono::steady_clock;
        static HotkeyService& Get();
        explicit HotkeyService(SettingsService& settings) : m_settings(settings) {}
        bool Configure(std::function<void()> schedule = {}); // Before ControlMap initialization; scheduler must defer.
        // Configure publishes once; views remain valid for this service's lifetime.
        std::span<const HotkeyAction> Actions() const;
        void SetAvailable(bool available);
        SettingsError Subscribe(std::string_view mod, std::string_view action, Callback callback, Subscription& out);
        SettingsError Unsubscribe(Subscription subscription);
        SettingsError AcquireBlock(HotkeyBlock& out);
        SettingsError ReleaseBlock(HotkeyBlock block);
        // Call vanilla first, then deliver its unconsumed named press events.
        void ProcessInput(const RE::InputEvent* head, const std::function<void()>& processOriginal);
        // Capture before native input processing. A remap/reset/block cancels
        // that generation, including observations not yet queued for delivery.
        std::uint64_t CurrentGeneration() const;
        void Activate(std::string_view nativeAction, std::uint64_t generation);
        void Invalidate();
        // Production caller verifies the native game-thread drain. Eligibility
        // is checked before each callback, with no service lock held.
        void Dispatch(const std::function<bool()>& eligible, Clock::time_point now = Clock::now());
    private:
        struct Listener
        {
            std::size_t action{};
            Callback callback;
            bool active{ true };
            std::thread::id invokingThread;
        };
        struct Pending
        {
            std::weak_ptr<Listener> listener;
            std::uint64_t generation{};
            Clock::time_point time;
        };
        std::expected<std::size_t, SettingsError> FindAction(std::string_view mod, std::string_view action) const;
        void InvalidatePending();
        SettingsService& m_settings;
        mutable std::mutex m_mutex;
        std::condition_variable m_callbackFinished;
        std::vector<HotkeyAction> m_actions;
        std::set<std::string, std::less<>> m_mods;
        std::map<Subscription, std::shared_ptr<Listener>> m_listeners;
        std::set<HotkeyBlock> m_blocks;
        std::vector<Pending> m_pending;
        Subscription m_nextSubscription{ 1 };
        HotkeyBlock m_nextBlock{ 1 };
        std::uint64_t m_generation{};
        std::function<void()> m_schedule;
        bool m_configured{}, m_available{};
        std::atomic_bool m_dispatching{};
    };
}
