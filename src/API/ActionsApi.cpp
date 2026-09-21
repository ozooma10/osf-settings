#include "SettingsApi.h"
#include "Actions/ActionService.h"

namespace OSFSettings::API
{
    namespace
    {
        Status Result(ActionError error)
        {
            switch (error) {
            case ActionError::None: return Status::Ok;
            case ActionError::NotReady: return Status::NotReady;
            case ActionError::InvalidArgument: return Status::InvalidArgument;
            case ActionError::UnknownAction: return Status::UnknownAction;
            case ActionError::AlreadyRegistered: return Status::AlreadyRegistered;
            case ActionError::UnknownInvocation: return Status::UnknownInvocation;
            default: return Status::InternalError;
            }
        }
    }

    Status SettingsApi::RegisterAction(const char* mod, const char* id, ActionFn callback, void* context) noexcept
    {
        if (!mod || !id || !callback) return Status::InvalidArgument;
        return Result(ActionService::Get().Register(mod, id,
            [callback, context](Invocation invocation, const std::string& modId, const std::string& actionId) {
                callback(invocation, modId.c_str(), actionId.c_str(), context);
            }));
    }

    Status SettingsApi::CompleteAction(Invocation invocation, bool succeeded, const char* message) noexcept
    {
        return Result(ActionService::Get().Complete(invocation, succeeded, message ? message : ""));
    }
}
