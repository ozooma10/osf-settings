#include "SettingsService.h"
#include "Localization.h"
#include "Harness/Performance.h"
#include "REX/LOG.h"

#include <utility>

namespace OSFSettings
{
    namespace
    {
        bool ValidKey(std::string_view key)
        {
            return !key.empty() && key.find('\0') == std::string_view::npos;
        }
    }

    SettingsService& SettingsService::Get()
    {
        static auto* service = new SettingsService;
        return *service;
    }

    void SettingsService::Load(const std::filesystem::path& schemas, const std::filesystem::path& values)
    {
        TestHarness::PerformanceScope profile(TestHarness::PerformancePoint::Load);
        std::lock_guard lock(m_mutex);
        if (m_loaded) return;
        m_store.LoadAll(schemas, values);
        m_loaded = true;
        ++m_schemaRevision;
    }

    bool SettingsService::Start()
    {
        std::lock_guard lock(m_mutex);
        if (!m_loaded) return false;
        m_ready.store(true, std::memory_order_release);
        return true;
    }

    bool SettingsService::IsReady() const noexcept { return m_ready.load(std::memory_order_acquire); }

    std::vector<ModSettings> SettingsService::Snapshot() const
    {
        TestHarness::PerformanceScope profile(TestHarness::PerformancePoint::Snapshot);
        std::lock_guard lock(m_mutex);
        std::vector<ModSettings> mods;
        mods.reserve(m_store.Mods().size());
        const auto catalog = Localization::Get();
        for (const auto& stored : m_store.Mods()) {
            auto& mod = mods.emplace_back(ModSettings{ stored.schema, stored.ResolvedValues() });
            catalog->Apply(mod.schema);
        }
        return mods;
    }

    std::optional<ModSettings> SettingsService::FindMod(std::string_view mod) const
    {
        TestHarness::PerformanceScope profile(TestHarness::PerformancePoint::FindMod);
        std::lock_guard lock(m_mutex);
        const auto* stored = m_store.FindMod(mod);
        if (!stored) return std::nullopt;
        ModSettings copy{ stored->schema, stored->ResolvedValues() };
        Localization::Get()->Apply(copy.schema);
        return copy;
    }

    void SettingsService::Localize(const std::filesystem::path& directory, std::string_view language)
    {
        TestHarness::PerformanceScope profile(TestHarness::PerformancePoint::Localize);
        std::lock_guard lock(m_mutex);
        Localization::Publish(std::make_shared<Localization::Catalog>(directory, language, m_store.Mods()));
        ++m_schemaRevision;
        for (const auto& mod : m_store.Mods()) {
            Notify(mod.schema.id, std::nullopt);
        }
    }

    std::vector<SettingsLoadError> SettingsService::LoadErrors() const
    {
        std::lock_guard lock(m_mutex);
        return m_store.LoadErrors();
    }

    std::expected<SettingValue, SettingsError> SettingsService::GetValue(std::string_view mod, std::string_view key) const
    {
        TestHarness::PerformanceScope profile(TestHarness::PerformancePoint::GetValue);
        if (!IsValidModId(mod) || !ValidKey(key)) return std::unexpected(SettingsError::InvalidArgument);
        std::lock_guard lock(m_mutex);
        if (!IsReady()) return std::unexpected(SettingsError::NotReady);
        const auto* stored = m_store.FindMod(mod);
        if (!stored) return std::unexpected(SettingsError::UnknownMod);
        const auto value = stored->GetValue(key);
        if (!value) return std::unexpected(SettingsError::UnknownSetting);
        return *value;
    }

    SettingsError SettingsService::FinishWrite(std::string_view mod, std::optional<std::string_view> key, const SettingsStore::SetResult& result) noexcept
    {
        if (result.ok()) {
            if (result.changed) Notify(mod, key);
            return SettingsError::None;
        }
        if (result.code == SettingsError::SaveFailed) {
            REX::ERROR("Settings: {}", result.error);
        }
        return result.code;
    }

    SettingsError SettingsService::SetValue(std::string_view mod, std::string_view key, SettingValue value)
    {
        if (!IsValidModId(mod) || !ValidKey(key)) return SettingsError::InvalidArgument;
        std::lock_guard lock(m_mutex);
        if (!IsReady()) return SettingsError::NotReady;
        return FinishWrite(mod, key, m_store.Set(mod, key, std::move(value)));
    }

    SettingsError SettingsService::Reset(std::string_view mod, std::string_view key)
    {
        if (!IsValidModId(mod) || !ValidKey(key)) return SettingsError::InvalidArgument;
        std::lock_guard lock(m_mutex);
        if (!IsReady()) return SettingsError::NotReady;
        return FinishWrite(mod, key, m_store.Reset(mod, key));
    }

    SettingsError SettingsService::ResetMod(std::string_view mod)
    {
        if (!IsValidModId(mod)) return SettingsError::InvalidArgument;
        std::lock_guard lock(m_mutex);
        if (!IsReady()) return SettingsError::NotReady;
        return FinishWrite(mod, std::nullopt, m_store.ResetMod(mod));
    }

    SettingsError SettingsService::Subscribe(std::string_view mod, Changed callback, Subscription& out)
    {
        if (!IsValidModId(mod) || !callback) return SettingsError::InvalidArgument;
        auto listener = std::make_shared<Listener>();
        listener->mod = mod;
        listener->callback = std::move(callback);
        std::lock_guard lock(m_mutex);
        if (!m_nextSubscription) return SettingsError::InternalError;
        m_listeners.emplace(m_nextSubscription, listener);
        out = m_nextSubscription++; // Publish the token before dispatch can begin.
        m_pending.store(true, std::memory_order_release);
        return SettingsError::None;
    }

    SettingsError SettingsService::RegisterProvider(ModSettings mod, SettingsStore::Save save, std::uint64_t& registration)
    {
        std::lock_guard lock(m_mutex);
        if (!IsReady()) return SettingsError::NotReady;
        const auto id = mod.schema.id;
        const auto result = m_store.RegisterProvider(std::move(mod), std::move(save), registration);
        if (result == SettingsError::None) {
            ++m_schemaRevision;
            Notify(id, std::nullopt);
        }
        return result;
    }

    SettingsError SettingsService::UnregisterProvider(std::uint64_t registration)
    {
        std::lock_guard lock(m_mutex);
        if (const auto id = m_store.UnregisterProvider(registration)) {
            ++m_schemaRevision;
            Notify(*id, std::nullopt);
            return SettingsError::None;
        }
        return SettingsError::InvalidArgument;
    }

    SettingsError SettingsService::Unsubscribe(Subscription subscription)
    {
        std::unique_lock lock(m_mutex);
        const auto found = m_listeners.find(subscription);
        if (found == m_listeners.end()) return SettingsError::UnknownSubscription;
        const auto listener = found->second;
        listener->active = false;
        m_listeners.erase(found);
        if (listener->invokingThread != std::this_thread::get_id()) {
            m_callbackFinished.wait(lock, [&] { return listener->invokingThread == std::thread::id{}; });
        }
        lock.unlock(); // Release callback captures outside the service lock.
        return SettingsError::None;
    }

    void SettingsService::Notify(std::string_view mod, std::optional<std::string_view> key) noexcept
    {
        ++m_revision;
        for (auto& [token, listener] : m_listeners) {
            if (listener->mod != mod) continue;
            if (!key) {
                listener->fullRefresh = true;
                listener->keys.clear();
            } else if (!listener->fullRefresh) {
                listener->keys.emplace(*key);
            }
            m_pending.store(true, std::memory_order_release);
        }
    }

    bool SettingsService::HasPendingChanges() const noexcept
    {
        return m_ready.load(std::memory_order_acquire) && m_pending.load(std::memory_order_acquire);
    }

    void SettingsService::Invoke(const std::shared_ptr<Listener>& listener, const std::string* key)
    {
        {
            std::lock_guard lock(m_mutex);
            if (!listener->active) return;
            listener->invokingThread = std::this_thread::get_id();
        }
        listener->callback({ listener->mod, key });
        {
            std::lock_guard lock(m_mutex);
            listener->invokingThread = {};
        }
        m_callbackFinished.notify_all();
    }

    void SettingsService::DispatchChanges() noexcept
    {
        TestHarness::PerformanceScope profile(TestHarness::PerformancePoint::Dispatch);
        if (m_dispatching.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        struct Finish
        {
            std::atomic_bool& dispatching;
            ~Finish() { dispatching.store(false, std::memory_order_release); }
        } finish{ m_dispatching };

        struct Batch
        {
            std::shared_ptr<Listener> listener;
            bool fullRefresh{};
            std::set<std::string, std::less<>> keys;
        };
        std::vector<Batch> batches;
        {
            std::lock_guard lock(m_mutex);
            if (!IsReady()) return;
            batches.reserve(m_listeners.size());
            for (const auto& [token, listener] : m_listeners) {
                if (listener->fullRefresh || !listener->keys.empty()) {
                    batches.push_back({ listener });
                }
            }
            for (auto& batch : batches) {
                batch.fullRefresh = std::exchange(batch.listener->fullRefresh, false);
                batch.keys.swap(batch.listener->keys);
            }
            m_pending.store(false, std::memory_order_release);
        }
        for (const auto& batch : batches) {
            if (batch.fullRefresh) {
                Invoke(batch.listener, nullptr);
            } else {
                for (const auto& key : batch.keys) {
                    Invoke(batch.listener, &key);
                }
            }
        }
    }
}
