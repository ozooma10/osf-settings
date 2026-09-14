#pragma once
#include <filesystem>

namespace OSFSettings::Paths
{
	bool Initialize();
	const std::filesystem::path& PluginDir();
	const std::filesystem::path& DataDir();
	std::filesystem::path SchemasDir();
	std::filesystem::path ValuesDir();
	std::filesystem::path LocalizationDir();
	std::filesystem::path StateDir();
	std::filesystem::path StarfieldUserDir();
}
