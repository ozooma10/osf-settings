#pragma once

#include "SettingsSchema.h"
#include <mutex>
#include <nlohmann/json.hpp>

namespace OSFSettings
{
    // OSF Settings' own values file, shared with its launcher history.
    class StateStore
    {
    public:
        StateStore(const std::filesystem::path& file, const std::filesystem::path& legacyHistory = {});
        nlohmann::json Read(std::string_view section = {}) const;
        bool Write(std::string_view section, const nlohmann::json& value, std::string& error);
        const std::filesystem::path& Path() const { return m_path; }
        const std::vector<SettingsLoadError>& LoadErrors() const { return m_errors; }

    private:
        mutable std::mutex m_mutex;
        std::filesystem::path m_path;
        nlohmann::json m_document;
        std::vector<SettingsLoadError> m_errors;
        std::string m_readError;
        bool m_imported{};
    };
}
