#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <filesystem>
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
        std::uint32_t recentOrder{}; // Zero means never opened; larger values are more recent.
    };
    class LauncherService
    {
    public:
        static LauncherService& Get();
        void Initialize(const std::vector<ModSettings>& mods);
        void LoadHistory(const std::filesystem::path& path);
        bool RecordOpened(std::string_view mod, std::string_view id);
        LauncherError Register(LaunchDestination destination);
        LauncherError SetAvailable(std::string_view mod, std::string_view id, bool available, std::string reason);
        std::vector<LaunchDestination> Snapshot() const;
        std::optional<LaunchDestination> Find(std::string_view mod, std::string_view id) const;
        std::uint64_t Revision() const { return m_revision.load(); }
    private:
        mutable std::mutex m_mutex;
        std::vector<LaunchDestination> m_destinations;
        std::filesystem::path m_historyPath;
        std::vector<std::pair<std::string, std::string>> m_recent;
        void SaveHistory() const; // Called with m_mutex held; writes cannot overtake one another.
        std::atomic_uint64_t m_revision{};
    };
}
