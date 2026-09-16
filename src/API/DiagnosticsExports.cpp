#include "DiagnosticsApi.h"

extern "C" __declspec(dllexport) void* OSFSettings_RequestDiagnosticsAPI(std::uint32_t version, std::uint32_t* outVersion) noexcept
{
    using namespace OSFSettings::API;
    if (outVersion) *outVersion = 0;
    if (!Supports(Diagnostics::kVersion, version)) {
        return nullptr;
    }
    auto* api = static_cast<Diagnostics::IDiagnostics*>(&DiagnosticsApi::Get());
    if (outVersion) {
        *outVersion = Diagnostics::kVersion;
    }
    return api;
}
