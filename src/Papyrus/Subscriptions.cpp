#include "Subscriptions.h"
#include "Values.h"
#include <algorithm>

namespace OSFSettings::Papyrus
{
    Subscriptions::Subscriptions(SettingsService& settings, HotkeyInputState& input, Dispatch dispatch) :
        m_settings(settings), m_input(input), m_state(std::make_shared<State>())
    {
        m_state->dispatch = std::move(dispatch);
    }

    Subscriptions::~Subscriptions() { Clear(); }

    SettingsError Subscriptions::Register(Receiver receiver, Kind kind, std::string mod, std::string key)
    {
        mod = FoldIdentifier(mod);
        if (receiver.script.empty() || !IsValidModId(mod) || (kind == Kind::Hotkey && key.empty())) {
            return SettingsError::InvalidArgument;
        }
        if (!m_settings.IsReady()) return SettingsError::NotReady;
        const auto mods = m_settings.Snapshot();
        const auto schema = std::ranges::find_if(mods, [&](const auto& item) { return item.schema.id == mod; });
        if (schema == mods.end()) {
            return SettingsError::UnknownMod;
        }
        if (kind == Kind::Hotkey) {
            const auto id = FoldIdentifier(key);
            const HotkeyDefinition* found{};
            for (const auto& hotkey : schema->schema.hotkeys) {
                if (FoldIdentifier(hotkey.id) != id) continue;
                if (found) return SettingsError::InvalidArgument;
                found = &hotkey;
            }
            if (!found) return SettingsError::UnknownHotkey;
            key = found->id;
        }

        std::lock_guard lock(m_state->mutex);
        if (m_state->suspended.load()) return SettingsError::NotReady;
        for (const auto& entry : m_state->entries) {
            if (entry->receiver == receiver && entry->kind == kind && entry->mod == mod && entry->key == key) return SettingsError::None;
        }
        auto entry = std::make_shared<Entry>();
        entry->receiver = std::move(receiver);
        entry->kind = kind;
        entry->mod = std::move(mod);
        entry->key = std::move(key);
        const std::weak_ptr<State> weak = m_state;
        SettingsError error;
        if (kind == Kind::Changes) {
            error = m_settings.Subscribe(entry->mod, [weak, entry](const SettingsService::Change& change) {
                Deliver(weak, entry, change.key ? *change.key : std::string{});
            }, entry->sourceToken);
        } else {
            error = m_input.Subscribe(entry->mod, entry->key, [weak, entry] {
                Deliver(weak, entry, entry->key);
            }, entry->sourceToken);
        }
        if (error != SettingsError::None) return error;
        m_state->entries.push_back(std::move(entry));
        return SettingsError::None;
    }

    void Subscriptions::Deliver(const std::weak_ptr<State>& weak, const std::shared_ptr<Entry>& entry, std::string key)
    {
        const auto state = weak.lock();
        if (!state) return;
        std::lock_guard gate(entry->dispatchMutex);
        if (!entry->active) return;
        if (state->suspended.load()) {
            if (entry->kind == Kind::Changes) {
                entry->refreshPending = true;
            }
            return;
        }
        if (std::exchange(entry->refreshPending, false)) {
            key.clear();
        }
        state->dispatch(entry->receiver, entry->kind, entry->mod, key);
    }

    void Subscriptions::Retire(const std::shared_ptr<Entry>& entry)
    {
        {
            // Wait for an active submission without holding the registry lock.
            std::lock_guard gate(entry->dispatchMutex);
            entry->active = false;
        }
        if (entry->kind == Kind::Changes) {
            m_settings.Unsubscribe(entry->sourceToken);
        } else {
            m_input.Unsubscribe(entry->sourceToken);
        }
    }

    bool Subscriptions::IsSuspended() const { return m_state->suspended.load(); }

    void Subscriptions::Suspend(std::uint8_t operation)
    {
        std::vector<std::shared_ptr<Entry>> entries;
        {
            std::lock_guard lock(m_state->mutex);
            ++m_state->transitions[operation];
            m_state->suspended.store(true);
            entries = m_state->entries;
        }
        for (const auto& entry : entries) { 
            std::lock_guard gate(entry->dispatchMutex); 
        }
    }

    void Subscriptions::Resume(std::uint8_t operation)
    {
        std::vector<std::shared_ptr<Entry>> entries;
        {
            std::lock_guard lock(m_state->mutex);
            const auto found = m_state->transitions.find(operation);
            if (found != m_state->transitions.end() && --found->second == 0) {
                m_state->transitions.erase(found);
            }
            m_state->suspended.store(!m_state->transitions.empty());
            if (m_state->suspended.load()) return;
            entries = m_state->entries;
        }
        for (const auto& entry : entries) {
            std::lock_guard gate(entry->dispatchMutex);
            if (entry->active && !m_state->suspended.load() && std::exchange(entry->refreshPending, false)) {
                m_state->dispatch(entry->receiver, entry->kind, entry->mod, {});
            }
        }
    }

    void Subscriptions::Clear()
    {
        std::vector<std::shared_ptr<Entry>> retired;
        {
            std::lock_guard lock(m_state->mutex);
            m_state->suspended.store(true);
            retired.swap(m_state->entries);
            m_state->transitions.clear();
        }
        for (const auto& entry : retired) {
            Retire(entry);
        }
    }
}
