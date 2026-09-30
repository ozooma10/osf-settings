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
    enum class LauncherError { None, InvalidArgument, AlreadyRegistered, UnknownLauncher, UnknownRequest };
    using LaunchCallback = std::function<void(const std::string&, const std::string&, std::uint64_t)>;
    struct LaunchDestination
    {
        std::string mod, id, modTitle, title, description, menu;
        LaunchCallback open;
        bool available{ true };
        std::string reason;
        std::uint32_t recentOrder{}; // Zero means never opened; larger values are more recent.
    };
    struct LaunchResult
    {
        LaunchCallback afterClose;
        std::string reason;
    };
    class LauncherService
    {
    public:
        static LauncherService& Get();
        void Initialize(const std::vector<ModSettings>& mods);
        void LoadHistory(const std::filesystem::path& directory);
        bool RecordOpened(std::string_view mod, std::string_view id);
        LauncherError Register(LaunchDestination destination);
        LauncherError SetAvailable(std::string_view mod, std::string_view id, bool available, std::string reason);
        // Creates the wait before invoking the copied callback, outside the registry lock.
        // Zero rejects a missing, unavailable, or native-menu destination.
        std::uint64_t BeginOpen(std::string_view mod, std::string_view id);
        LauncherError Complete(std::uint64_t requestId, LaunchCallback afterClose, std::string reason);
        std::optional<LaunchResult> TakeResult(std::uint64_t requestId);
        void EndOpen(std::uint64_t requestId);
        std::vector<LaunchDestination> Snapshot() const;
        std::optional<LaunchDestination> Find(std::string_view mod, std::string_view id) const;
        std::uint64_t Revision() const { return m_revision.load(); }
    private:
        mutable std::mutex m_mutex;
        std::vector<LaunchDestination> m_destinations;
        struct Request
        {
            std::uint64_t requestId{};
            bool completed{};
            std::optional<LaunchResult> result;
        };
        std::optional<Request> m_request;
        std::uint64_t m_nextRequestId{};
        std::filesystem::path m_dataPath;
        bool m_historyDirty{};
        std::vector<std::pair<std::string, std::string>> m_recent;
        void SaveHistory(); // Called with m_mutex held; writes cannot overtake one another.
        bool RecordOpenedLocked(std::string_view mod, std::string_view id);
        std::atomic_uint64_t m_revision{};
    };
}
