#include "SettingsApi.h"
#include "Negotiate.h"

extern "C" __declspec(dllexport) void* OSFSettings_RequestAPI(std::uint32_t version, std::uint32_t* outVersion) noexcept
{
    using namespace OSFSettings::API;
    return Negotiate(kVersion, version, outVersion, static_cast<ISettings*>(&SettingsApi::Get()));
}
