#include "SettingsApi.h"

extern "C" __declspec(dllexport) void* OSFSettings_RequestAPI(std::uint32_t version, std::uint32_t* outVersion) noexcept
{
    using namespace OSFSettings;
    if (outVersion) *outVersion = 0;
    if (!API::Supports(API::kVersion, version)) {
        return nullptr;
    }
    auto* api = static_cast<API::ISettings*>(&API::SettingsApi::Get());
    if (outVersion) {
        *outVersion = API::kVersion;
    }
    return api;
}
