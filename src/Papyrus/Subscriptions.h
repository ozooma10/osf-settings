#pragma once

#include "Input/HotkeyInputState.h"
#include "Settings/SettingsService.h"

namespace OSFSettings::Papyrus
{
    // Weak instance identity: no VM pointers, script objects, or type-info references.
    // A zero handle selects a Global function. Script names are folded by the VM adapter.
    struct Receiver
    {
        std::uint64_t handle{};
        std::string script;
        bool operator==(const Receiver&) const = default;
    };

    class Subscriptions
    {
    public:
        enum class Kind { Changes, Hotkey };
        using Dispatch = std::function<void(const Receiver&, Kind, const std::string& mod, const std::string& key)>;

        Subscriptions(SettingsService& settings, HotkeyInputState& input, Dispatch dispatch);
        ~Subscriptions();
        Subscriptions(const Subscriptions&) = delete;
        Subscriptions& operator=(const Subscriptions&) = delete;

        SettingsError Register(Receiver receiver, Kind kind, std::string mod, std::string key = {});
        bool IsSuspended() const;
        void Suspend(std::uint8_t operation = 0); // Remember changes; ignore hotkeys until every pending operation ends.
        void Resume(std::uint8_t operation = 0);
        void Clear(); // Invalidate every identity; remains suspended until Resume.

    private:
        struct Entry
        {
            Receiver receiver;
            Kind kind{};
            std::string mod, key;
            std::uint64_t sourceToken{};
            std::recursive_mutex dispatchMutex;
            bool active{ true };
            bool refreshPending{};
        };
        struct State
        {
            std::mutex mutex;
            std::vector<std::shared_ptr<Entry>> entries;
            Dispatch dispatch;
            std::atomic_bool suspended{};
            std::map<std::uint8_t, std::uint32_t> transitions;
        };
        static void Deliver(const std::weak_ptr<State>& state, const std::shared_ptr<Entry>& entry, std::string key);
        void Retire(const std::shared_ptr<Entry>& entry);

        SettingsService& m_settings;
        HotkeyInputState& m_input;
        std::shared_ptr<State> m_state;
    };
}
