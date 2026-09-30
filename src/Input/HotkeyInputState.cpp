#include "HotkeyInputState.h"
#include "Settings/SettingValue.h"

#include <utility>

namespace OSFSettings
{
    HotkeyInputState& HotkeyInputState::Get()
    {
        static auto* state = new HotkeyInputState;
        return *state;
    }

    void HotkeyInputState::Initialize(Declarations declarations)
    {
        std::lock_guard lock(m_mutex);
        m_declarations = std::move(declarations);
        m_initialized = true;
    }

    SettingsError HotkeyInputState::Register(std::string_view mod, std::string_view id, Callback callback, void* context)
    {
        if (mod.empty() || id.empty() || !callback) return SettingsError::InvalidArgument;
        std::lock_guard lock(m_mutex);
        const auto result = Validate(mod, id);
        if (result != SettingsError::None) return result;
        m_callbacks[NativeHotkeys::EventName(mod, id)].push_back({ callback, context });
        return SettingsError::None;
    }

    SettingsError HotkeyInputState::Validate(std::string_view mod, std::string_view id) const
    {
        if (!m_initialized) return SettingsError::NotReady;
        const auto foundMod = m_declarations.find(mod);
        if (foundMod == m_declarations.end()) return SettingsError::UnknownMod;
        const auto found = foundMod->second.find(id);
        if (found == foundMod->second.end()) return SettingsError::UnknownHotkey;
        if (found->second == Target::Menu) return SettingsError::TypeMismatch;
        if (found->second == Target::Invalid) return SettingsError::InvalidValue;
        return SettingsError::None;
    }

    SettingsError HotkeyInputState::Subscribe(std::string_view mod, std::string_view id, std::function<void()> callback, Subscription& out)
    {
        if (mod.empty() || id.empty() || !callback) return SettingsError::InvalidArgument;
        std::lock_guard lock(m_mutex);
        const auto result = Validate(mod, id);
        if (result != SettingsError::None) return result;
        if (!m_nextSubscription) return SettingsError::InternalError;
        auto observer = std::make_shared<Observer>();
        observer->action = NativeHotkeys::EventName(mod, id);
        observer->callback = std::move(callback);
        m_observers.emplace(m_nextSubscription, std::move(observer));
        out = m_nextSubscription++;
        return SettingsError::None;
    }

    bool HotkeyInputState::Unsubscribe(Subscription subscription)
    {
        std::lock_guard lock(m_mutex);
        const auto found = m_observers.find(subscription);
        if (found == m_observers.end()) return false;
        found->second->active.store(false);
        m_observers.erase(found);
        return true;
    }

    HotkeyInputState::Block HotkeyInputState::AcquireBlock()
    {
        std::lock_guard lock(m_mutex);
        const auto block = m_nextBlock++;
        m_blocks.insert(block);
        m_pressed.clear();
        return block;
    }

    bool HotkeyInputState::ReleaseBlock(Block block)
    {
        std::lock_guard lock(m_mutex);
        return m_blocks.erase(block) != 0;
    }

    bool HotkeyInputState::Blocked()
    {
        std::lock_guard lock(m_mutex);
        return !m_blocks.empty();
    }

    void HotkeyInputState::ResetHeldButtons()
    {
        std::lock_guard lock(m_mutex);
        m_pressed.clear();
    }

    bool HotkeyInputState::ProcessButton(std::uint32_t key, const NativeHotkeys::Action& action, float value, float heldSeconds,
        std::uint32_t device, std::uint32_t instance)
    {
        if (device == 0) {
            if (!key || key >= KeyBinding::Unbound) return false;
        } else if (device == 2) {
            // Native controller masks, plus the engine's two trigger IDs.
            switch (key) {
            case 1: case 2: case 4: case 8: case 9: case 10:
            case 0x10: case 0x20: case 0x40: case 0x80: case 0x100: case 0x200:
            case 0x1000: case 0x2000: case 0x4000: case 0x8000: break;
            default: return false;
            }
        } else return false;
        const std::array identity{ device, instance, key };

        std::unique_lock lock(m_mutex);
        if (!m_blocks.empty()) return false;
        if (!action.menu) {
            if (!(value > 0) || heldSeconds != 0) return false;
            const auto callbacks = m_callbacks.find(action.event);
            std::vector<Listener> listeners;
            if (callbacks != m_callbacks.end()) {
                listeners = callbacks->second;
            }
            std::vector<std::shared_ptr<Observer>> observers;
            for (const auto& [token, observer] : m_observers) {
                if (observer->action == action.event) {
                    observers.push_back(observer);
                }
            }
            if (listeners.empty() && observers.empty()) return false;
            lock.unlock();
            for (const auto& observer : observers) {
                if (observer->active.load()) {
                    observer->callback();
                }
            }
            for (const auto& listener : listeners) {
                listener.callback(action.mod.c_str(), action.id.c_str(), listener.context);
            }
            return true;
        }

        if (value > 0) {
            if (heldSeconds == 0) {
                m_pressed.insert_or_assign(identity, action.event);
            }
            return false;
        }

        const auto press = m_pressed.find(identity);
        if (press == m_pressed.end()) return false;
        const bool activate = value == 0 && heldSeconds >= 0 && press->second == action.event;
        m_pressed.erase(press);
        return activate;
    }
}
