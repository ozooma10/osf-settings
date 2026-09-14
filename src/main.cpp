#include "Core/Plugin.h"

SFSE_PLUGIN_PRELOAD(const SFSE::PreLoadInterface* a_sfse)
{
	SFSE::Init(a_sfse, { .logLevel = REX::ELogLevel::Debug, .logRotate = 1, .hook = false });
	return true;
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse, { .hook = false });
	return OSFSettings::Plugin::OnLoad();
}
