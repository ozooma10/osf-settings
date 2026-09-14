#include "Paths.h"

namespace OSFSettings::Paths
{
	namespace
	{
		std::filesystem::path g_pluginDir;
		std::filesystem::path g_dataDir;
	}

	bool Initialize()
	{
		const std::filesystem::path gamePath{ REX::FModule::GetExecutingModule().GetFileName() };
		g_pluginDir = gamePath.parent_path() / "Data" / "SFSE" / "Plugins";
		g_dataDir = g_pluginDir / "OSF" / "Settings";
		return true;
	}

	const std::filesystem::path& PluginDir() { return g_pluginDir; }
	const std::filesystem::path& DataDir() { return g_dataDir; }
	std::filesystem::path SchemasDir() { return g_dataDir / "schemas"; }
	std::filesystem::path ValuesDir() { return g_dataDir / "values"; }
}
