#include "Paths.h"

namespace OSFSettings::Paths
{
	namespace
	{
		std::filesystem::path g_dataDir;
	}

	bool Initialize()
	{
		const std::filesystem::path gamePath{ REX::FModule::GetExecutingModule().GetFileName() };
		g_dataDir = gamePath.parent_path() / "Data" / "SFSE" / "Plugins" / "OSF" / "SettingsSlim";
		return true;
	}

	std::filesystem::path SchemasDir() { return g_dataDir / "schemas"; }
}
