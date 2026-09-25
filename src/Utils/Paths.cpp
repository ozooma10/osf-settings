#include "Paths.h"
#include "Harness/TestHarness.h"
#include "SFSE/Logger.h"

namespace OSFSettings::Paths
{
	namespace
	{
		std::filesystem::path g_dataDir;
		std::filesystem::path g_userDataDir;
		std::filesystem::path g_valuesDir;
	}

	bool Initialize()
	{
		try {
			const std::filesystem::path gamePath{ REX::FModule::GetExecutingModule().GetFileName() };
			const auto dataDir = gamePath.parent_path() / "Data" / "SFSE" / "Plugins" / "OSF" / "Settings";
			std::filesystem::path valuesDir;
			if (!TestHarness::InitializeValues(gamePath.parent_path(), valuesDir)) return false;

			// Harness storage stays separate from the player's files.
			if (valuesDir.empty()) {
				const auto logs = SFSE::log::log_directory();
				if (!logs) {
					REX::ERROR("Cannot resolve the SFSE log directory for settings");
					return false;
				}
				valuesDir = logs->parent_path().parent_path() / "OSF" / "Settings";
			}
			g_dataDir = dataDir;
			g_userDataDir = valuesDir;
			g_valuesDir = valuesDir;
			REX::INFO("Settings user data: {}", g_userDataDir.string());
			return true;
		} catch (const std::exception& error) {
			REX::ERROR("Cannot initialize settings paths: {}", error.what());
			return false;
		}
	}

	std::filesystem::path SchemasDir() { return g_dataDir / "schemas"; }
	std::filesystem::path UserDataDir() { return g_userDataDir; }
	std::filesystem::path ValuesDir() { return g_valuesDir; }
	std::filesystem::path LocalizationDir() { return g_dataDir / "translations"; }
}
