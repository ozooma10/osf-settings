#include "Settings/Localization.h"
#include "ActionService.h"
#include <algorithm>
#include <utility>

namespace OSFSettings
{
    ActionService& ActionService::Get()
    {
        static auto* service = new ActionService;
        return *service;
    }

    void ActionService::Initialize(const std::vector<ModSettings>& mods)
    {
        std::lock_guard lock(m_mutex);
        if (m_ready) return;
        for (const auto& mod : mods) {
            for (const auto& group : mod.schema.groups) {
                for (const auto& control : group.controls) {
                    if (const auto* action = std::get_if<ActionDefinition>(&control)) {
                        m_actions.try_emplace(Key{ mod.schema.id, action->id });
                    }
                }
            }
        }
        m_ready = true;
        m_revision++;
    }

    ActionError ActionService::Register(std::string_view mod, std::string_view id, Callback callback, bool sessionScoped)
    {
        if (!IsValidModId(mod) || id.empty() || !callback) {
            return ActionError::InvalidArgument;
        }
        std::lock_guard lock(m_mutex);
        if (!m_ready || !m_transitions.empty()) return ActionError::NotReady;
        const auto found = m_actions.find({ std::string(mod), std::string(id) });
        if (found == m_actions.end()) return ActionError::UnknownAction;
        auto& entry = found->second;
        if (entry.callback) return ActionError::AlreadyRegistered;
        entry.callback = std::move(callback);
        entry.sessionScoped = sessionScoped;
        m_revision++;
        return ActionError::None;
    }

    ActionError ActionService::Invoke(std::string_view mod, std::string_view id, Invocation& out)
    {
        Callback callback;
        const Key key{ mod, id };
        Invocation invocation;
        {
            std::lock_guard lock(m_mutex);
            if (!m_ready || !m_transitions.empty()) return ActionError::NotReady;
            const auto found = m_actions.find(key);
            if (found == m_actions.end()) return ActionError::UnknownAction;
            auto& entry = found->second;
            if (!entry.callback) return ActionError::NotReady;
            if (entry.invocation) return ActionError::Busy;
            if (!m_nextInvocation) return ActionError::NotReady; // Exhaustion must never reuse an old token.
            callback = entry.callback;
            invocation = entry.invocation = m_nextInvocation++;
            entry.state = ActionState::Running;
            entry.message.clear();
            out = invocation;
            m_revision++;
        }
        // The handler must submit lengthy work and return promptly. No store lock is held.
        callback(invocation, key.first, key.second);
        return ActionError::None;
    }

    ActionError ActionService::Complete(Invocation invocation, bool succeeded, std::string message)
    {
        if (!invocation || !IsValidString(message, 4096)) return ActionError::InvalidArgument;
        std::lock_guard lock(m_mutex);
        const auto found = std::ranges::find_if(m_actions, [&](const auto& item) { return item.second.invocation == invocation; });
        if (found == m_actions.end()) return ActionError::UnknownInvocation;
        auto& entry = found->second;
        entry.message = std::move(message);
        entry.state = succeeded ? ActionState::Succeeded : ActionState::Failed;
        entry.invocation = 0;
        m_revision++;
        return ActionError::None;
    }

    ActionStatus ActionService::Status(std::string_view mod, std::string_view id) const
    {
        std::lock_guard lock(m_mutex);
        const auto found = m_actions.find({ std::string(mod), std::string(id) });
        if (found == m_actions.end()) return { false, ActionState::Ready, tr("actions.unknown") };
        const auto& entry = found->second;
        if (!m_transitions.empty()) return { false, entry.state, tr("actions.sessionChanging") };
        if (!entry.callback) return { false, entry.state, tr("actions.noHandler") };
        auto message = entry.message;
        if (message.empty()) {
            switch (entry.state) {
            case ActionState::Ready: message = tr("actions.ready"); break;
            case ActionState::Running: message = tr("actions.working"); break;
            case ActionState::Succeeded: message = tr("actions.completed"); break;
            case ActionState::Failed: message = tr("actions.failed"); break;
            }
        }
        return { !entry.invocation, entry.state, std::move(message) };
    }

    void ActionService::Suspend(std::uint8_t operation)
    {
        std::lock_guard lock(m_mutex);
        m_transitions[operation]++;
        m_revision++;
    }

    void ActionService::Resume(std::uint8_t operation)
    {
        std::lock_guard lock(m_mutex);
        const auto found = m_transitions.find(operation);
        if (found != m_transitions.end() && --found->second == 0) {
            m_transitions.erase(found);
        }
        m_revision++;
    }

    void ActionService::ClearSession()
    {
        std::lock_guard lock(m_mutex);
        for (auto& [key, entry] : m_actions) {
            if (entry.sessionScoped) {
                entry.callback = {};
            }
            entry.invocation = 0;
            entry.state = ActionState::Ready;
            entry.message.clear();
        }
        m_transitions.clear();
        m_revision++;
    }
}
