#include "HotkeyService.h"
#include "SFSE/Impl/PCH.h"
#include "RE/B/BSInputEventUser.h"

namespace OSFSettings
{
    HotkeyService& HotkeyService::Get()
    {
        static auto* service = new HotkeyService(SettingsService::Get());
        return *service;
    }
    bool HotkeyService::Configure(std::function<void()> schedule)
    {
        const auto mods = m_settings.Snapshot();
        std::vector<HotkeyAction> actions;
        std::set<std::string, std::less<>> modIds;
        for (const auto& mod : mods) {
            modIds.insert(mod.schema.id);
            for (const auto& definition : mod.schema.hotkeys) {
                if (actions.size() == kMaxHotkeyActions) return false;
                actions.push_back({ mod.schema.id, definition.id, NativeHotkeyName(mod.schema.id, definition.id),
                    mod.schema.title + ": " + definition.label, definition.defaultKey });
            }
        }
        std::lock_guard lock(m_mutex);
        if (m_configured) return false;
        m_actions = std::move(actions);
        m_mods = std::move(modIds);
        m_schedule = std::move(schedule);
        m_configured = true;
        return true;
    }
    std::span<const HotkeyAction> HotkeyService::Actions() const
    {
        std::lock_guard lock(m_mutex);
        return m_actions;
    }
    void HotkeyService::SetAvailable(bool available)
    {
        std::lock_guard lock(m_mutex);
        m_available = available;
        if (!available) InvalidatePending();
    }
    std::expected<std::size_t, SettingsError> HotkeyService::FindAction(std::string_view mod, std::string_view action) const
    {
        if (!IsValidModId(mod) || !IsValidHotkeyId(action)) return std::unexpected(SettingsError::InvalidArgument);
        if (!m_configured || !m_available) return std::unexpected(SettingsError::NotReady);
        if (!m_mods.contains(mod)) return std::unexpected(SettingsError::UnknownMod);
        for (std::size_t i = 0; i < m_actions.size(); ++i) {
            if (m_actions[i].mod == mod && m_actions[i].id == action) return i;
        }
        return std::unexpected(SettingsError::UnknownAction);
    }
    SettingsError HotkeyService::Subscribe(std::string_view mod, std::string_view action, Callback callback, Subscription& out)
    {
        if (!callback) return SettingsError::InvalidArgument;
        std::lock_guard lock(m_mutex);
        const auto index = FindAction(mod, action);
        if (!index) return index.error();
        if (!m_nextSubscription) return SettingsError::InternalError;
        auto listener = std::make_shared<Listener>();
        listener->action = *index;
        listener->callback = std::move(callback);
        m_listeners.emplace(m_nextSubscription, std::move(listener));
        out = m_nextSubscription++;
        return SettingsError::None;
    }
    SettingsError HotkeyService::Unsubscribe(Subscription subscription)
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
        lock.unlock();
        return SettingsError::None;
    }
    std::uint64_t HotkeyService::CurrentGeneration() const
    {
        std::lock_guard lock(m_mutex);
        return m_generation;
    }

    void HotkeyService::InvalidatePending()
    {
        ++m_generation;
        m_pending.clear();
    }
    void HotkeyService::Invalidate()
    {
        std::lock_guard lock(m_mutex);
        InvalidatePending();
    }
    SettingsError HotkeyService::AcquireBlock(HotkeyBlock& out)
    {
        std::lock_guard lock(m_mutex);
        if (!m_nextBlock) return SettingsError::InternalError;
        m_blocks.insert(m_nextBlock);
        out = m_nextBlock++;
        InvalidatePending();
        return SettingsError::None;
    }
    SettingsError HotkeyService::ReleaseBlock(HotkeyBlock block)
    {
        std::lock_guard lock(m_mutex);
        return m_blocks.erase(block) ? SettingsError::None : SettingsError::UnknownHotkeyBlock;
    }
    void HotkeyService::ProcessInput(const RE::InputEvent* head, const std::function<void()>& processOriginal)
    {
        const auto generation = CurrentGeneration();
        processOriginal();
        // Vanilla owns press/repeat state and resolves the binding to an action.
        // Read the final native result while the event batch is still borrowed.
        for (auto* event = head; event; event = event->next) {
            const auto* button = event->AsButtonEvent();
            if (button && button->deviceType == RE::InputEvent::DeviceType::kKeyboard &&
                button->IsPressed() && !button->disabled && event->status != RE::InputEvent::Status::kStop) {
                Activate(std::string_view{ button->QUserEvent() }, generation);
            }
        }
    }
    void HotkeyService::Activate(std::string_view nativeAction, std::uint64_t generation)
    {
        std::unique_lock lock(m_mutex);
        if (!m_available || !m_blocks.empty() || generation != m_generation) return;
        const bool wasEmpty = m_pending.empty();
        for (const auto& [token, listener] : m_listeners) {
            if (m_pending.size() == 256) break;
            if (m_actions[listener->action].eventName == nativeAction) {
                m_pending.push_back({ listener, generation, Clock::now() });
            }
        }
        auto schedule = wasEmpty && !m_pending.empty() ? m_schedule : std::function<void()>{};
        lock.unlock();
        if (schedule) schedule();
    }
    void HotkeyService::Dispatch(const std::function<bool()>& eligible, Clock::time_point now)
    {
        if (m_dispatching.exchange(true, std::memory_order_acq_rel)) return;
        struct Finish
        {
            std::atomic_bool& flag;
            ~Finish() { flag.store(false, std::memory_order_release); }
        } finish{ m_dispatching };
        std::vector<Pending> pending;
        {
            std::lock_guard lock(m_mutex);
            pending.swap(m_pending);
        }
        for (const auto& event : pending) {
            if (!eligible()) {
                Invalidate();
                return;
            }
            const auto listener = event.listener.lock();
            if (!listener) continue;
            {
                std::lock_guard lock(m_mutex);
                if (!listener->active || !m_available || !m_blocks.empty() ||
                    event.generation != m_generation || now - event.time > std::chrono::milliseconds(250)) continue;
                listener->invokingThread = std::this_thread::get_id();
            }
            const auto& action = m_actions[listener->action];
            try { listener->callback(action.mod, action.id); }
            catch (...) { /* Release waiters even if an internal callback violates its no-throw contract. */ }
            {
                std::lock_guard lock(m_mutex);
                listener->invokingThread = {};
            }
            m_callbackFinished.notify_all();
        }
    }
}
