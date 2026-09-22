#include "LauncherService.h"
#include "Settings/SettingsSchema.h"
#include <utility>

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
    std::vector<LaunchDestination> LauncherService::Snapshot() const { std::lock_guard lock(m_mutex); return m_destinations; }
    std::optional<LaunchDestination> LauncherService::Find(std::string_view mod, std::string_view id) const
    {
        std::lock_guard lock(m_mutex);
        for (const auto& entry : m_destinations) {
            if (entry.mod == mod && entry.id == id) return entry;
        }
        return std::nullopt;
    }
}
