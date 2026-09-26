#include "KeyActions.h"
#include "HotkeyInputState.h"
#include "Settings/SettingsService.h"

namespace OSFSettings
{
    KeyActions& KeyActions::Get() { static auto* actions = new KeyActions; return *actions; }
    std::uint64_t KeyActions::Subscribe(std::string mod, std::string key, std::function<void()> callback)
    {
        std::lock_guard lock(m_mutex);
        if (!m_next) return 0;
        const auto token = m_next++;
        m_listeners.emplace(token, std::make_shared<Listener>(Listener{ std::move(mod), std::move(key), std::move(callback) }));
        return token;
    }
    bool KeyActions::Unsubscribe(std::uint64_t token)
    {
        std::lock_guard lock(m_mutex);
        const auto found = m_listeners.find(token);
        if (found == m_listeners.end()) return false;
        found->second->active = false;
        m_listeners.erase(found);
        return true;
    }
    void KeyActions::Process(std::uint32_t key)
    {
        if (!key || key >= KeyBinding::Unbound) return;
        std::lock_guard lock(m_mutex);
        std::vector<std::shared_ptr<Listener>> listeners;
        for (const auto& [token, listener] : m_listeners) listeners.push_back(listener);
        for (const auto& listener : listeners) {
            if (HotkeyInputState::Get().Blocked()) break;
            if (!listener->active) continue;
            const auto value = SettingsService::Get().GetValue(listener->mod, listener->key);
            const auto* binding = value ? std::get_if<KeyBinding>(&*value) : nullptr;
            if (binding && binding->keyCode == key) listener->callback();
        }
    }
}
