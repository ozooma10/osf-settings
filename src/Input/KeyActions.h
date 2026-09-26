#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace OSFSettings
{
    // Optional observers of settings-backed keys, independent of ControlMap actions.
    class KeyActions
    {
    public:
        static KeyActions& Get();
        std::uint64_t Subscribe(std::string mod, std::string key, std::function<void()> callback);
        bool Unsubscribe(std::uint64_t token);
        void Process(std::uint32_t key);
    private:
        struct Listener { std::string mod, key; std::function<void()> callback; bool active{ true }; };
        std::recursive_mutex m_mutex;
        std::map<std::uint64_t, std::shared_ptr<Listener>> m_listeners;
        std::uint64_t m_next{ 1 };
    };
}
