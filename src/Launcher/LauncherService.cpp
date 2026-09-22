#include "LauncherService.h"
#include "Settings/SettingsSchema.h"
#include <utility>
#include <algorithm>
#include "Settings/StateStore.h"

namespace OSFSettings
{
    namespace
    {
        bool ValidID(std::string_view id)
        {
            return !id.empty() && IsValidString(id, 256);
        }
    }
    LauncherService& LauncherService::Get() { static auto* service = new LauncherService; return *service; }

    void LauncherService::Initialize(const std::vector<ModSettings>& mods)
    {
        for (const auto& mod : mods) {
            for (const auto& menu : mod.schema.menus) {
                if (Register({ mod.schema.id, menu.id, mod.schema.title, menu.title, menu.description, menu.menu, {} }) != LauncherError::None) {
                    REX::WARN("Launcher {}/{}: could not register schema menu", mod.schema.id, menu.id);
                }
            }
        }
    }

    LauncherError LauncherService::Register(LaunchDestination destination)
    {
        if (!IsValidModId(destination.mod) || destination.mod.size() > 128 || !ValidID(destination.id) || destination.title.empty() || !IsValidString(destination.title, 256) || !IsValidString(destination.modTitle, 256) ||
            !IsValidString(destination.description, 4096) || !IsValidString(destination.reason, 4096) || (destination.menu.empty() == !destination.open) || !IsValidString(destination.menu, 256) || destination.menu == "OSFSettingsMenu") {
                return LauncherError::InvalidArgument;
        }
        if (destination.modTitle.empty()) {
            destination.modTitle = destination.mod;
        }
        std::lock_guard lock(m_mutex);
        for (const auto& entry : m_destinations) {
            if (entry.mod == destination.mod && entry.id == destination.id) return LauncherError::AlreadyRegistered;
        }
        m_destinations.push_back(std::move(destination));
        m_revision++;
        return LauncherError::None;
    }
    LauncherError LauncherService::SetAvailable(std::string_view mod, std::string_view id, bool available, std::string reason)
    {
        if (!IsValidString(reason, 4096)) return LauncherError::InvalidArgument;
        std::lock_guard lock(m_mutex);
        for (auto& entry : m_destinations) {
            if (entry.mod != mod || entry.id != id) continue;
            entry.available = available;
            entry.reason = std::move(reason);
            ++m_revision;
            return LauncherError::None;
        }
        return LauncherError::NotFound;
    }
    void LauncherService::LoadHistory(std::shared_ptr<StateStore> state)
    {
        std::lock_guard lock(m_mutex);
        m_state = std::move(state);
        m_historyDirty = false;
        m_recent.clear();
        m_revision++;
        if (!m_state) return;
        try {
            const auto document = m_state->Read("launcher");
            if (document.is_null()) return;
            if (!document.at("formatVersion").is_number_integer() || document.at("formatVersion") != 1 || !document.at("recent").is_array() || document.at("recent").size() > 256){
                throw std::runtime_error("invalid history format");
            }
            std::vector<std::pair<std::string, std::string>> recent;
            for (const auto& item : document.at("recent")) {
                auto identity = std::pair{ item.at("mod").get<std::string>(), item.at("id").get<std::string>() };
                if (!IsValidModId(identity.first) || identity.first.size() > 128 || !ValidID(identity.second)) {
                    throw std::runtime_error("invalid destination identity");
                }
                if (std::ranges::find(recent, identity) == recent.end()) {
                    recent.push_back(std::move(identity));
                }
            }
            m_recent = std::move(recent);
        } catch (const std::exception& error) {
            REX::WARN("Launcher history {}: {}", m_state->Path().string(), error.what());
        }
    }

    void LauncherService::SaveHistory()
    {
        if (!m_state || !m_historyDirty) return;
        try {
            auto recent = nlohmann::json::array();
            for (const auto& [mod, id] : m_recent) recent.push_back({{"mod", mod}, {"id", id}});
            std::string error;
            if (!m_state->Write("launcher", {{"formatVersion", 1}, {"recent", recent}}, error)) {
                REX::WARN("Launcher history {}: {}", m_state->Path().string(), error);
                return;
            }
            m_historyDirty = false;
        } catch (const std::exception& error) {
            REX::WARN("Launcher history {}: {}", m_state->Path().string(), error.what());
        }
    }

    bool LauncherService::RecordOpened(std::string_view mod, std::string_view id)
    {
        std::lock_guard lock(m_mutex);
        const auto entry = std::ranges::find_if(m_destinations, [&](const auto& value) { return value.mod == mod && value.id == id; });
        if (entry == m_destinations.end() || !entry->available) return false;
        const auto identity = std::pair{ std::string(mod), std::string(id) };
        if (!m_recent.empty() && m_recent.front() == identity) {
            SaveHistory(); // Retry a failed write even when recency did not change.
            return true;
        }
        std::erase(m_recent, identity);
        m_recent.insert(m_recent.begin(), identity);
        if (m_recent.size() > 256) {
            m_recent.resize(256);
        }
        m_revision++;
        m_historyDirty = true;
        SaveHistory();
        return true;
    }

    std::vector<LaunchDestination> LauncherService::Snapshot() const
    {
        std::lock_guard lock(m_mutex);
        auto result = m_destinations;
        for (auto& entry : result) {
            const auto found = std::ranges::find(m_recent, std::pair{ entry.mod, entry.id });
            entry.recentOrder = found == m_recent.end() ? 0 : static_cast<std::uint32_t>(m_recent.end() - found);
        }
        return result;
    }
    std::optional<LaunchDestination> LauncherService::Find(std::string_view mod, std::string_view id) const
    {
        std::lock_guard lock(m_mutex);
        for (const auto& entry : m_destinations) {
            if (entry.mod == mod && entry.id == id) return entry;
        }
        return std::nullopt;
    }
}
