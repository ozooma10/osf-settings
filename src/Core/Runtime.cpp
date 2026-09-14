#include "Runtime.h"
#include "Utils/Paths.h"

#include <exception>
#include <spdlog/spdlog.h>

namespace OSFSettings
{
    Runtime &Runtime::Get()
    {
        static Runtime instance;
        return instance;
    }

    bool Runtime::Initialize()
    {
        if (m_initialized) return true;

        try {
            if (!Paths::Initialize()) return false;
            const auto schemaDir = Paths::SchemasDir();
            REX::INFO("Loading schemas from {}", schemaDir.string());
            m_settings.LoadAll(schemaDir, Paths::ValuesDir());

            for (const auto& error : m_settings.LoadErrors()) {
                REX::ERROR("Settings {}: {}", error.file.string(), error.message);
            }

            std::size_t settingCount = 0;
            for (const auto& mod : m_settings.Mods()) {
                for (const auto& [key, value] : mod.values) {
                    REX::INFO("Loaded {} / {} = {} (current value)", mod.schema.id, key, value);
                    ++settingCount;
                }
            }
            REX::INFO("Checkpoint 2: {} mod(s), {} setting(s), {} load error(s)", m_settings.Mods().size(), settingCount, m_settings.LoadErrors().size());
            m_initialized = true;
            return true;
        } catch (const std::exception& error) {
            REX::ERROR("OSF Settings Slim initialization failed: {}", error.what());
            if (auto logger = spdlog::default_logger()) logger->flush();
            return false;
        }
    }
}
