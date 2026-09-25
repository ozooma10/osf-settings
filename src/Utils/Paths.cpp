#include "Paths.h"
#include "Harness/TestHarness.h"
#include "REX/W32/OLE32.h"
#include "REX/W32/SHELL32.h"

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

			std::filesystem::path userDataDir;
			if (!valuesDir.empty()) {
				// Keep harness storage separate from the player's files.
				userDataDir = valuesDir.parent_path();
			} else {
				wchar_t* buffer{};
				const auto result = REX::W32::SHGetKnownFolderPath(REX::W32::FOLDERID_Documents,
					REX::W32::KF_FLAG_DEFAULT, nullptr, &buffer);
				const std::unique_ptr<wchar_t[], decltype(&REX::W32::CoTaskMemFree)> documents(buffer, REX::W32::CoTaskMemFree);
				if (result != 0 || !documents) {
					REX::ERROR("Cannot resolve the Documents folder for settings: HRESULT {}", result);
					return false;
				}
				userDataDir = std::filesystem::path(documents.get()) / "My Games" / "Starfield" / "OSF" / "Settings";
				valuesDir = userDataDir / "values";
			}
			g_dataDir = dataDir;
			g_userDataDir = userDataDir;
			g_valuesDir = valuesDir;
			REX::INFO("Settings user data: {}", g_userDataDir.string());
			REX::INFO("Settings values: {}", g_valuesDir.string());
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
