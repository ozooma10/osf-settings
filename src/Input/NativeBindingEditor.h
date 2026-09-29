#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

namespace RE { class InputEvent; class BSInputEventSingleUser; }

namespace OSFSettings
{
    // Menu-owned input handoff. The native Settings data model owns the binding transaction.
    class NativeBindingEditor
    {
    public:
        enum class InputResult { Unhandled, Handled, Cancelled };
        static bool Install();
        bool Begin(bool gamepad);
        InputResult ProcessInput(const RE::InputEvent* event);
        void End(bool cancel);
        static bool IsActive() { return s_active.load(); }
        static bool CapturesGamepad() { return s_gamepad.load(); }

    private:
        inline static std::atomic_bool s_active{};
        inline static std::atomic_bool s_gamepad{};
        std::mutex m_mutex;
        RE::BSInputEventSingleUser* m_input{};
        std::uint64_t m_hotkeyBlock{};
    };
}
