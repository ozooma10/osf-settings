#include "HotkeyInputState.h"
#include "SFSE/API.h"

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

    SettingsError HotkeyInputState::Register(std::string_view mod, std::string_view id, Callback callback, void* user)
    {
        if (mod.empty() || id.empty() || !callback) return SettingsError::InvalidArgument;
        std::lock_guard lock(m_mutex);
        if (!m_initialized) return SettingsError::NotReady;
        const auto foundMod = m_declarations.find(mod);
        if (foundMod == m_declarations.end()) return SettingsError::UnknownMod;
        const auto found = foundMod->second.find(id);
        if (found == foundMod->second.end()) return SettingsError::UnknownHotkey;
        if (found->second == Target::Menu) return SettingsError::TypeMismatch;
        if (found->second == Target::Invalid) return SettingsError::InvalidValue;
        m_callbacks[std::string(mod) + "/" + std::string(id)].push_back({ callback, user });
        return SettingsError::None;
    }

    HotkeyInputState::Block HotkeyInputState::AcquireBlock()
    {
        std::lock_guard lock(m_mutex);
        if (!m_nextBlock) return 0; // Never recycle a token after wraparound.
        const auto block = m_nextBlock;
        m_blocks.insert(block);
        m_nextBlock++;
        m_pressed.clear();
        return block;
    }

    bool HotkeyInputState::ReleaseBlock(Block block)
    {
        std::lock_guard lock(m_mutex);
        return m_blocks.erase(block) != 0;
    }

    bool HotkeyInputState::ProcessButton(std::uint32_t key, std::string_view action, float value, float heldSeconds)
    {
        constexpr std::uint32_t kUnbound = 0xFF;
        if (!key || key >= kUnbound || action.empty()) return false;

        std::unique_lock lock(m_mutex);
        if (!m_blocks.empty()) return false;
        const auto separator = action.find('/');
        if (separator == std::string_view::npos) return false;
        const auto mod = m_declarations.find(action.substr(0, separator));
        if (mod == m_declarations.end()) return false;
        const auto hotkey = mod->second.find(action.substr(separator + 1));
        if (hotkey == mod->second.end() || hotkey->second == Target::Invalid) return false;

        if (hotkey->second == Target::Callback) {
            if (!(value > 0) || heldSeconds != 0) return false;
            const auto callbacks = m_callbacks.find(action);
            if (callbacks == m_callbacks.end()) return false;
            auto task = [listeners = callbacks->second, mod = mod->first, id = hotkey->first] {
                for (const auto& listener : listeners) {
                    listener.callback(mod.c_str(), id.c_str(), listener.user);
                }
            };
            lock.unlock();
            SFSE::GetTaskInterface()->AddTask(std::move(task));
            return true;
        }

        if (value > 0) {
            if (heldSeconds == 0) {
                m_pressed.insert_or_assign(key, action);
            }
            return false;
        }

        const auto press = m_pressed.find(key);
        if (press == m_pressed.end()) return false;
        const bool activate = value == 0 && heldSeconds >= 0 && press->second == action;
        m_pressed.erase(press);
        return activate;
    }
}
