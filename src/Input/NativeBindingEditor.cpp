#include "NativeBindingEditor.h"
#include "RE/C/ControlMap.h"
#include "RE/S/SettingsDataModel.h"
#include "REL/THook.h"
#include "REX/W32/USER32.h"
#include <optional>

namespace OSFSettings
{
    namespace
    {
        constexpr std::ptrdiff_t kValidateCandidateCall = 0x185;
        constexpr std::uint8_t kAllowed = 0;
        constexpr std::uint8_t kConfirmConflict = 2;
        constexpr std::uint32_t kUnbound = 0xFF;
        using Context = RE::ControlMap::InputContextID;
        using Slot = RE::ControlMap::BindingSlot;
        using Device = RE::InputEvent::DeviceType;
        using ValidateHook = REL::THook<std::uint8_t(RE::ControlMap*, const RE::BSFixedString*, std::uint8_t, const std::uint32_t*, Slot, Context)>;
        std::optional<ValidateHook> g_validateHook;

        std::uint8_t ValidateCandidate(RE::ControlMap* map, const RE::BSFixedString* action, std::uint8_t device, const std::uint32_t* keys, Slot slot, Context context)
        {
            const auto result = (*g_validateHook)(map, action, device, keys, slot, context);
            // Vanilla permits occupied-key swaps. Only OSF's active PC capture asks for confirmation as well; native rejection and commit rules remain intact.
            if (result != kAllowed || !NativeBindingEditor::IsActive() || context != Context::kMainGameplay || device > static_cast<std::uint8_t>(Device::kMouse) || keys[0] == kUnbound) {
                return result;
            }
            for (const auto& mapping : map->GetMappings(context, static_cast<Device>(device))) {
                if (mapping.keyCode == keys[0] && mapping.modifierKeyCode == keys[1] && mapping.visibleInControls && mapping.eventID != *action) {
                    return kConfirmConflict;
                }
            }
            return result;
        }
    }

    bool NativeBindingEditor::Install()
    {
        if (g_validateHook) {
            return g_validateHook->GetEnabled();
        }
        g_validateHook.emplace("NativeBindings::Validate", RE::ID::SettingsDataModel::EvaluateRemapCandidate, kValidateCandidateCall, &ValidateCandidate);
        if (!g_validateHook->Init() || !g_validateHook->Enable()) {
            g_validateHook.reset();
            return false;
        }
        return true;
    }

    bool NativeBindingEditor::Begin()
    {
        std::lock_guard lock(m_mutex);
        if (m_input || !g_validateHook || !g_validateHook->GetEnabled()) return false;
        m_input = RE::SettingsDataModel::GetSingleton();
        s_active = m_input != nullptr;
        return m_input != nullptr;
    }

    NativeBindingEditor::InputResult NativeBindingEditor::ProcessInput(const RE::InputEvent* event)
    {
        std::unique_lock lock(m_mutex);
        if (!m_input || !event || event->status == RE::InputEvent::Status::kStop) return InputResult::Unhandled;
        if (event->eventType == RE::InputEvent::EventType::kButton && event->deviceType == RE::InputEvent::DeviceType::kKeyboard) {
            const auto* button = static_cast<const RE::ButtonEvent*>(event);
            if (button->idCode == REX::W32::VK_ESCAPE) {
                lock.unlock();
                End(true);
                const_cast<RE::InputEvent*>(event)->status = RE::InputEvent::Status::kStop;
                return InputResult::Cancelled;
            }
        }
        // Pause/Main Menu may already have delivered this event to the same native receiver.
        if (m_input->currInputTimeCount != -1 && event->timeCode < static_cast<std::uint32_t>(m_input->currInputTimeCount)) {
            return InputResult::Unhandled;
        }
        if (!m_input->ShouldHandleEvent(event)) {
            return InputResult::Unhandled;
        }
        m_input->DispatchEvent(event);
        const_cast<RE::InputEvent*>(event)->status = RE::InputEvent::Status::kStop;
        return InputResult::Handled;
    }

    void NativeBindingEditor::End(bool cancel)
    {
        std::lock_guard lock(m_mutex);
        if (!m_input) return;
        if (cancel) {
            RE::SettingsDataModel::CancelRemap();
        }
        m_input = nullptr;
        s_active = false;
    }
}
