#include "../../sdk/OSFSettings_Launcher.h"
#include "Launcher/LauncherService.h"

namespace OSFSettings::API::Launcher
{
    namespace
    {
        Status Convert(LauncherError error)
        {
            switch (error) {
                case LauncherError::None:
                    return Status::Ok;
                case LauncherError::InvalidArgument:
                    return Status::InvalidArgument;
                case LauncherError::AlreadyRegistered:
                    return Status::AlreadyRegistered;
                case LauncherError::NotFound:
                    return Status::NotFound;
            }
            return Status::InternalError;
        }
        class LauncherApi final : public ILauncher
        {
            Status Register(const Destination& value) noexcept override
            {
                if (!value.modId || !value.id || !value.title) return Status::InvalidArgument;
                LaunchDestination entry{ value.modId, value.id, value.modTitle ? value.modTitle : "", value.title, value.description ? value.description : "", value.menu ? value.menu : "", {} };
                if (value.open) {
                    entry.open = [fn = value.open, context = value.context](const auto& mod, const auto& id, std::uint64_t requestId) {
                        fn(mod.c_str(), id.c_str(), requestId, context);
                    };
                }
                return Convert(LauncherService::Get().Register(std::move(entry)));
            }
            Status SetAvailable(const char* mod, const char* id, bool available, const char* reason) noexcept override
            {
                if (!mod || !id) return Status::InvalidArgument;
                return Convert(LauncherService::Get().SetAvailable(mod, id, available, reason ? reason : ""));
            }
            Status Complete(std::uint64_t requestId, OpenFn afterClose, void* context, const char* reason) noexcept override
            {
                LaunchCallback callback;
                if (afterClose) {
                    callback = [afterClose, context](const auto& mod, const auto& id, std::uint64_t request) {
                        afterClose(mod.c_str(), id.c_str(), request, context);
                    };
                }
                return Convert(LauncherService::Get().Complete(requestId, std::move(callback), reason ? reason : ""));
            }
        };
    }
    ILauncher* GetLauncherApi() { static auto* api = new LauncherApi; return api; }
}
extern "C" __declspec(dllexport) void* OSFSettings_RequestLauncherAPI(std::uint32_t version, std::uint32_t* outVersion) noexcept
{
    using namespace OSFSettings::API::Launcher;
    if (outVersion) *outVersion = 0;
    if ((version >> 16) != (kVersion >> 16) || version > kVersion) return nullptr;
    auto* api = GetLauncherApi();
    if (outVersion) {
        *outVersion = kVersion;
    }
    return api;
}
