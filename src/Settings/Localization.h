#pragma once

#include "SettingsSchema.h"

#include <initializer_list>
#include <map>
#include <memory>
#include <span>

namespace OSFSettings::Localization
{
    using Messages = std::map<std::string, std::string, std::less<>>;

    std::string NormalizeLanguage(std::string_view language);
    std::string Format(std::string_view text, std::initializer_list<std::pair<std::string_view, std::string_view>> arguments);

    class Catalog
    {
    public:
        Catalog();
        Catalog(const std::filesystem::path& directory, std::string_view language, std::span<const ModSettings> mods);
        void Apply(ModSchema& schema) const;
        const Messages& UI() const { return m_ui; }
        const std::string& Language() const { return m_language; }
        const std::vector<SettingsLoadError>& Errors() const { return m_errors; }

    private:
        Messages m_ui;
        std::map<std::string, ModSchema, std::less<>> m_schemas;
        std::string m_language{ "en" };
        std::vector<SettingsLoadError> m_errors;
    };

    std::shared_ptr<const Catalog> Get();
    void Publish(std::shared_ptr<const Catalog> catalog);
    std::string Text(std::string_view key);
    std::string Text(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> arguments);
    void Initialize(); // Engine startup, after the stock translation resource loads.
}

namespace OSFSettings
{
    inline std::string tr(std::string_view key)
    {
        return Localization::Text(key);
    }

    inline std::string tr(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> arguments)
    {
        return Localization::Text(key, arguments);
    }
}
