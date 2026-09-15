#include "KeyCapture.h"
#include "KeyNames.h"
#include "REX/W32/USER32.h"

namespace OSFSettings
{
    bool KeyCapture::IsActive() const { return m_state != State::Idle; }

    void KeyCapture::BeginCapture()
    {
        std::lock_guard lock(m_mutex);
        m_state = State::WaitingForKey;
        m_selectedKeyCode = KeyBinding::Unbound;
        m_selectedKeyReleased = false;
    }

    void KeyCapture::EndCapture()
    {
        std::lock_guard lock(m_mutex);
        m_state = State::Idle;
        // Continue swallowing any key held at confirmation/cancellation through its release.
    }

    void KeyCapture::ResetForMenuClose()
    {
        std::lock_guard lock(m_mutex);
        m_state = State::Idle;
        m_heldKeys.reset(); // A closed menu will not receive the remaining key releases.
    }

    void KeyCapture::RequestCancel()
    {
        std::lock_guard lock(m_mutex);
        if (IsActive()) m_state = State::Cancelled;
    }

    void KeyCapture::RetryConfirmation()
    {
        std::lock_guard lock(m_mutex);
        if (m_state == State::ConfirmationRequested) m_state = State::KeySelected;
    }

    bool KeyCapture::ShouldConsumeKey(std::uint32_t keyCode) const
    {
        std::lock_guard lock(m_mutex);
        return IsActive() || (keyCode < m_heldKeys.size() && m_heldKeys[keyCode]);
    }

    bool KeyCapture::HandleKeyEvent(std::uint32_t keyCode, bool isDown, bool isRepeat)
    {
        std::lock_guard lock(m_mutex);
        const bool wasHeld = keyCode < m_heldKeys.size() && m_heldKeys[keyCode];
        if (!IsActive() && !wasHeld) {
            return false;
        }
        if (keyCode < m_heldKeys.size()) {
            m_heldKeys[keyCode] = isDown;
        }
        if (!isDown) {
            if (keyCode == m_selectedKeyCode) {
                m_selectedKeyReleased = true;
            }
            return true;
        }
        if (isRepeat || wasHeld) {
            return true;
        }
        if (keyCode == REX::W32::VK_ESCAPE) {
            m_state = State::Cancelled;
        } else if (m_state == State::WaitingForKey) {
            if (IsBindableKey(keyCode)) {
                m_selectedKeyCode = keyCode;
                m_selectedKeyReleased = false;
                m_state = State::KeySelected;
            }
        } else if (m_state == State::KeySelected && m_selectedKeyReleased && keyCode == REX::W32::VK_RETURN) {
            m_state = State::ConfirmationRequested;
        }
        return true;
    }

    KeyCapture::Snapshot KeyCapture::GetSnapshot() const
    {
        std::lock_guard lock(m_mutex);
        return { m_state, m_selectedKeyCode, m_selectedKeyReleased };
    }
}
