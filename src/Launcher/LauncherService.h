#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OSFSettings
{
    struct ModSettings;
    enum class LauncherError { None, InvalidArgument, AlreadyRegistered, NotFound };
    struct LaunchDestination
    {
        std::string mod, id, modTitle, title, description, menu;
        std::function<void(const std::string&, const std::string&)> open;
        bool available{ true };
        std::string reason;
    };
    class LauncherService
    {
    public:
        static LauncherService& Get();
        void Initialize(const std::vector<ModSettings>& mods);
        LauncherError Register(LaunchDestination destination);
        LauncherError SetAvailable(std::string_view mod, std::string_view id, bool available, std::string reason);
        std::vector<LaunchDestination> Snapshot() const;
        std::optional<LaunchDestination> Find(std::string_view mod, std::string_view id) const;
        std::uint64_t Revision() const { return m_revision.load(); }
    private:
        mutable std::mutex m_mutex;
        std::vector<LaunchDestination> m_destinations;
        std::atomic_uint64_t m_revision{};
    };
}
