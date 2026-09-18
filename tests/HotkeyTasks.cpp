#include "SFSE/Impl/PCH.h"
#include "SFSE/API.h"
#include "HotkeyTasks.h"

// Native fixtures use CommonLibSF's real task wrapper with a host-owned queue.
namespace SFSE
{
    const TaskInterface* GetTaskInterface() noexcept
    {
        static const Impl::SFSETaskInterface tasks{ 1, +[](void* task) {
            auto* delegate = static_cast<ITaskDelegate*>(task);
            HotkeyTasks::Add([delegate] { delegate->Run(); delegate->Destroy(); });
        }, nullptr };
        return reinterpret_cast<const TaskInterface*>(&tasks);
    }
}
