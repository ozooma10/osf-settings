#pragma once

#include "../../HotkeyTasks.h"

// Only the portable callback target uses this header in place of CommonLibSF.
namespace SFSE
{
    class TaskInterface
    {
    public:
        void AddTask(std::function<void()> task) const { HotkeyTasks::Add(std::move(task)); }
    };

    inline const TaskInterface* GetTaskInterface() noexcept
    {
        static const TaskInterface tasks;
        return &tasks;
    }
}
