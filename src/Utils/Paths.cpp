#include "Paths.h"
#include "harness/TestHarness.h"

namespace OSFSettings::Paths
{
	namespace
	{
		std::filesystem::path g_dataDir;
		std::filesystem::path g_valuesDir;
	}

	bool Initialize()
	{
		const std::filesystem::path gamePath{ REX::FModule::GetExecutingModule().GetFileName() };
		g_dataDir = gamePath.parent_path() / "Data" / "SFSE" / "Plugins" / "OSF" / "Settings";
		g_valuesDir = g_dataDir / "values";
		return TestHarness::InitializeValues(gamePath.parent_path(), g_valuesDir);
	}

	std::filesystem::path SchemasDir() { return g_dataDir / "schemas"; }
	std::filesystem::path ValuesDir() { return g_valuesDir; }
}
