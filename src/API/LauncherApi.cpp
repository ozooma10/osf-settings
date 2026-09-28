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
                LaunchDestination entry{ value.modId, value.id, value.modTitle ? value.modTitle : "", value.title, value.description ? value.description : "", value.menu ? value.menu : "", {}, {} };
                if (value.open) {
                    entry.open = [fn = value.open, context = value.context](const auto& mod, const auto& id) {
                        fn(mod.c_str(), id.c_str(), context);
                    };
                }
                return Convert(LauncherService::Get().Register(std::move(entry)));
            }
            Status SetAvailable(const char* mod, const char* id, bool available, const char* reason) noexcept override
            {
                if (!mod || !id) return Status::InvalidArgument;
                return Convert(LauncherService::Get().SetAvailable(mod, id, available, reason ? reason : ""));
            }
            Status SetPrepare(const char* mod, const char* id, PrepareFn prepare, void* context) noexcept override
            {
                if (!mod || !id) return Status::InvalidArgument;
                std::function<void(const std::string&, const std::string&)> step;
                if (prepare) {
                    step = [prepare, context](const auto& mod, const auto& id) { prepare(mod.c_str(), id.c_str(), context); };
                }
                return Convert(LauncherService::Get().SetPrepare(mod, id, std::move(step)));
            }
            Status ReportPrepared(const char* mod, const char* id, bool ready, const char* reason) noexcept override
            {
                if (!mod || !id) return Status::InvalidArgument;
                return Convert(LauncherService::Get().ReportPrepared(mod, id, ready, reason ? reason : ""));
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
