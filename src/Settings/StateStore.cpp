#include "StateStore.h"
#include "Persistence/AtomicFile.h"

#include <fstream>

namespace OSFSettings
{
    namespace
    {
        constexpr std::uintmax_t MaxBytes = 16 * 1024 * 1024;
        nlohmann::json ReadFile(const std::filesystem::path& path)
        {
            if (std::filesystem::file_size(path) > MaxBytes) throw std::runtime_error("state file is too large");
            std::ifstream input(path, std::ios::binary);
            if (!input) throw std::runtime_error("cannot read state file");
            return nlohmann::json::parse(input, [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
                if (depth > 64) throw std::runtime_error("state nesting is too deep");
                return true;
            });
        }
        bool VersionOne(const nlohmann::json& document)
        {
            return document.is_object() && document.contains("formatVersion") &&
                document["formatVersion"].is_number_integer() && document["formatVersion"] == 1;
        }
    }

    StateStore::StateStore(const std::filesystem::path& file, const std::filesystem::path& legacyHistory) :
        m_path(file), m_document{{"formatVersion", 1}, {"values", nlohmann::json::object()}}
    {
        try {
            if (std::filesystem::exists(file)) {
                auto document = ReadFile(file);
                if (!VersionOne(document) || !document.contains("values") || !document["values"].is_object()) {
                    throw std::runtime_error("unsupported or invalid values format");
                }
                m_document = std::move(document);
            }
        } catch (const std::exception& error) {
            m_readError = error.what();
            m_errors.push_back({file, m_readError});
            return; // Preserve unreadable/newer data; writes must not erase it.
        }

        if (legacyHistory.empty() || m_document.contains("launcher")) return;
        try {
            if (!std::filesystem::exists(legacyHistory)) return;
            auto history = ReadFile(legacyHistory);
            if (!VersionOne(history) || !history.contains("recent") || !history["recent"].is_array()) {
                throw std::runtime_error("invalid legacy history format");
            }
            m_document["launcher"] = std::move(history);
            m_imported = true;
        } catch (const std::exception& error) { m_errors.push_back({legacyHistory, error.what()}); }
    }

    nlohmann::json StateStore::Read(std::string_view section) const
    {
        std::lock_guard lock(m_mutex);
        if (section.empty()) return m_document;
        const auto found = m_document.find(section);
        if (found == m_document.end()) return nullptr;
        return *found;
    }

    bool StateStore::Write(std::string_view section, const nlohmann::json& value, std::string& error)
    {
        std::lock_guard lock(m_mutex);
        error.clear();
        if (!m_readError.empty()) { error = m_path.string() + ": " + m_readError; return false; }
        try {
            auto proposed = m_document;
            proposed[section] = value;
            if (!m_imported && proposed == m_document) return true;
            const auto bytes = proposed.dump(2) + '\n';
            if (bytes.size() > MaxBytes) throw std::runtime_error("state file is too large");
            if (!Persistence::WriteAtomic(m_path, std::as_bytes(std::span(bytes.data(), bytes.size())), error)) return false;
            m_document.swap(proposed);
            m_imported = false;
            return true;
        } catch (const std::exception& exception) { error = exception.what(); return false; }
    }
}
