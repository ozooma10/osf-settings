#pragma once

#include "Settings/SettingsSchema.h"
#include <atomic>
#include <functional>
#include <map>
#include <mutex>

namespace OSFSettings
{
    enum class ActionError { None, NotReady, InvalidArgument, UnknownAction, AlreadyRegistered, Busy, UnknownInvocation };
    enum class ActionState { Ready, Running, Succeeded, Failed };

    struct ActionStatus
    {
        bool available{};
        ActionState state{};
        std::string message;
    };

    class ActionService
    {
    public:
        using Invocation = std::uint64_t;
        using Callback = std::function<void(Invocation, const std::string& mod, const std::string& id)>;
        static ActionService& Get();
        void Initialize(const std::vector<ModSettings>& mods);
        ActionError Register(std::string_view mod, std::string_view id, Callback callback, bool sessionScoped = false);
        ActionError Begin(std::string_view mod, std::string_view id, Invocation& out);
        void Dispatch(Invocation invocation) noexcept; // Called once by the task dispatcher, never by the movie.
        ActionError Complete(Invocation invocation, bool succeeded, std::string message);
        ActionStatus Status(std::string_view mod, std::string_view id) const;
        std::uint64_t Revision() const noexcept { return m_revision.load(); }

        void Suspend(std::uint8_t operation);
        void Resume(std::uint8_t operation);
        void ClearSession(); // Keeps native handlers, retires Papyrus handlers and all invocation tokens.

    private:
        using Key = std::pair<std::string, std::string>;
        struct Entry
        {
            Callback callback;
            bool sessionScoped{};
            Invocation invocation{};
            bool submitted{};
            ActionState state{};
            std::string message;
        };
        std::recursive_mutex m_dispatchMutex;
        mutable std::mutex m_mutex;
        std::map<Key, Entry> m_actions;
        std::map<std::uint8_t, std::uint32_t> m_transitions;
        Invocation m_nextInvocation{ 1 }; // Never reset or reused during this process.
        std::atomic_uint64_t m_revision{};
        bool m_ready{};
    };
}
