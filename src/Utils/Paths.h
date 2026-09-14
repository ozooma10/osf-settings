#pragma once
#include <filesystem>

namespace OSFSettings::Paths
{
	bool Initialize();
	std::filesystem::path SchemasDir();
}
