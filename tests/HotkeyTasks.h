#pragma once

#include <functional>
#include <utility>
#include <vector>

// Host substitute for SFSE task submission. The test chooses when queued tasks run.
namespace HotkeyTasks
{
    inline std::vector<std::function<void()>> pending;

    inline void Add(std::function<void()> task) { pending.push_back(std::move(task)); }

    inline void Run()
    {
        auto tasks = std::exchange(pending, {});
        for (auto& task : tasks) task();
    }
}
