#include "HotkeyInputState.h"

namespace OSFSettings
{
    HotkeyInputState& HotkeyInputState::Get()
    {
        static auto* state = new HotkeyInputState;
        return *state;
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

        std::lock_guard lock(m_mutex);
        if (!m_blocks.empty()) return false;
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
