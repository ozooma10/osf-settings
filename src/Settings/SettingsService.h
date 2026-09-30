#pragma once

#include "SettingsStore.h"

#include <atomic>
#include <condition_variable>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace OSFSettings
{
    class SettingsService
    {
    public:
        using Subscription = std::uint64_t;
        struct Change
        {
            const std::string& mod;
            const std::string* key; // Null requests a full refresh. Strings last through the callback.
        };
        using Changed = std::function<void(const Change&)>; // Callbacks must not throw.

        static SettingsService& Get();

        void Load(const std::filesystem::path& schemas, const std::filesystem::path& values);
        bool Start(); // After the notification dispatcher is installed. False when Load has not run.
        void Localize(const std::filesystem::path& directory, std::string_view language);
        std::vector<ModSettings> Snapshot() const;
        std::optional<ModSettings> FindMod(std::string_view mod) const; // Localized copy of one mod.
        std::vector<SettingsLoadError> LoadErrors() const;
        bool HasPendingChanges() const noexcept;
        void DispatchChanges() noexcept;

        bool IsReady() const noexcept;
        std::expected<SettingValue, SettingsError> GetValue(std::string_view mod, std::string_view key) const;
        SettingsError SetValue(std::string_view mod, std::string_view key, SettingValue value);
        SettingsError Reset(std::string_view mod, std::string_view key);
        SettingsError ResetMod(std::string_view mod);
        SettingsError RegisterProvider(ModSettings mod, SettingsStore::Save save, std::uint64_t& registration);
        SettingsError UnregisterProvider(std::uint64_t registration);
        std::uint64_t Revision() const noexcept { return m_revision.load(); }
        SettingsError Subscribe(std::string_view mod, Changed callback, Subscription& out);
        SettingsError Unsubscribe(Subscription subscription);

    private:
        struct Listener
        {
            std::string mod;
            Changed callback;
            bool active{ true };
            bool fullRefresh{ true };
            std::set<std::string, std::less<>> keys;
            std::thread::id invokingThread;
        };

        SettingsError FinishWrite(std::string_view mod, std::optional<std::string_view> key, const SettingsStore::SetResult& result) noexcept;
        void Notify(std::string_view mod, std::optional<std::string_view> key) noexcept;
        void Invoke(const std::shared_ptr<Listener>& listener, const std::string* key);

        mutable std::mutex m_mutex;
        std::condition_variable m_callbackFinished;
        SettingsStore m_store;
        std::map<Subscription, std::shared_ptr<Listener>> m_listeners;
        Subscription m_nextSubscription{ 1 };
        bool m_loaded{};
        std::atomic_bool m_ready{};
        std::atomic_bool m_pending{};
        std::atomic_bool m_dispatching{};
        std::atomic_uint64_t m_revision{};
    };
}
