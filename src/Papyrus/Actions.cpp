#include "Actions.h"

namespace OSFSettings::Papyrus
{
    ActionError Actions::Register(Receiver receiver, std::string mod, std::string id)
    {
        if (receiver.script.empty()) return ActionError::InvalidArgument;
        std::lock_guard lock(m_mutex);
        for (const auto& entry : m_entries) {
            if (entry.mod == mod && entry.id == id) {
                return entry.receiver == receiver ? ActionError::None : ActionError::AlreadyRegistered;
            }
        }
        // Reserve backing storage first: registration must not succeed without its identity record.
        m_entries.reserve(m_entries.size() + 1);
        const auto result = m_service.Register(mod, id,
            [receiver, dispatch = m_dispatch, service = &m_service](auto invocation, const auto& modId, const auto& actionId) {
                if (!dispatch(receiver, invocation, modId, actionId)) {
                    service->Complete(invocation, false, "The script receiver is unavailable or the VM rejected the action.");
                }
            }, true);
        if (result == ActionError::None) {
            m_entries.push_back({ std::move(receiver), std::move(mod), std::move(id) });
        }
        return result;
    }

    void Actions::ClearSession()
    {
        std::lock_guard lock(m_mutex);
        m_service.ClearSession();
        m_entries.clear();
    }
}
