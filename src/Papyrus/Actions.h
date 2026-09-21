#pragma once

#include "Actions/ActionService.h"
#include "Subscriptions.h"

namespace OSFSettings::Papyrus
{
    class Actions
    {
    public:
        using Dispatch = std::function<bool(const Receiver&, ActionService::Invocation, const std::string&, const std::string&)>;
        Actions(ActionService& service, Dispatch dispatch) : m_service(service), m_dispatch(std::move(dispatch)) {}
        ActionError Register(Receiver receiver, std::string mod, std::string id);
        void ClearSession();
    private:
        struct Entry { Receiver receiver; std::string mod, id; };
        ActionService& m_service;
        Dispatch m_dispatch;
        std::mutex m_mutex;
        std::vector<Entry> m_entries;
    };
}
