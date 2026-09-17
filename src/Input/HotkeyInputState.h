#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

namespace OSFSettings
{
    class HotkeyInputState
    {
    public:
        using Block = std::uint64_t;

        static HotkeyInputState& Get();
        Block AcquireBlock();
        bool ReleaseBlock(Block block);

        bool ProcessButton(std::uint32_t key, std::string_view action, float value, float heldSeconds);

    private:
        std::mutex m_mutex;
        std::set<Block> m_blocks;
        Block m_nextBlock{ 1 };
        std::map<std::uint32_t, std::string> m_pressed;
    };
}
