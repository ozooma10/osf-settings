#pragma once

#include <bitset>
#include <cstdint>
#include <mutex>
#include "Settings/SettingValue.h"

namespace OSFSettings
{
    class KeyCapture
    {
    public:
        enum class State { Idle, WaitingForKey, KeySelected, ConfirmationRequested, Cancelled };
        struct Snapshot { State state; std::uint32_t selectedKeyCode; bool selectedKeyReleased; };
        void BeginCapture();
        void EndCapture();
        void ResetForMenuClose();
        void RequestCancel();
        void RetryConfirmation();
        bool ShouldConsumeKey(std::uint32_t keyCode) const;
        bool HandleKeyEvent(std::uint32_t keyCode, bool isDown, bool isRepeat);
        Snapshot GetSnapshot() const;

    private:
        bool IsActive() const;
        mutable std::mutex m_mutex;
        State m_state{ State::Idle };
        std::uint32_t m_selectedKeyCode{ KeyBinding::Unbound };
        bool m_selectedKeyReleased{};
        std::bitset<256> m_heldKeys;
    };
}
