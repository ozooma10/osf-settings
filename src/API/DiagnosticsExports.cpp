#include "DiagnosticsApi.h"
#include "Negotiate.h"

extern "C" __declspec(dllexport) void* OSFSettings_RequestDiagnosticsAPI(std::uint32_t version, std::uint32_t* outVersion) noexcept
{
    using namespace OSFSettings::API;
    return Negotiate(Diagnostics::kVersion, version, outVersion, static_cast<Diagnostics::IDiagnostics*>(&DiagnosticsApi::Get()));
}
