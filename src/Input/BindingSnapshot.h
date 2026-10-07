#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace OSFSettings
{
    struct BindingRecord
    {
        std::string action;
        std::uint32_t context{}, device{}, slot{}, key{}, modifier{};
        bool visibleInControls{ true };
    };

    class BindingSnapshot
    {
    public:
        enum class Status { Loading, Ready, Unavailable };
        struct Value
        {
            std::uint32_t generation{};
            Status status{ Status::Unavailable };
            std::vector<BindingRecord> records;
        };

        std::uint32_t Begin()
        {
            std::scoped_lock lock(m_mutex);
            m_value = { m_value.generation + 1, Status::Loading, {} };
            return m_value.generation;
        }
        void Publish(std::uint32_t generation, Status status, std::vector<BindingRecord> records = {})
        {
            std::scoped_lock lock(m_mutex);
            if (generation == m_value.generation) {
                m_value = { generation, status, std::move(records) };
            }
        }
        Value Read() const
        {
            std::scoped_lock lock(m_mutex);
            return m_value;
        }
        void Invalidate()
        {
            std::scoped_lock lock(m_mutex);
            m_value = { m_value.generation + 1, Status::Unavailable, {} };
        }

    private:
        mutable std::mutex m_mutex;
        Value m_value;
    };

    std::uint32_t RequestBindingSnapshot(const std::shared_ptr<BindingSnapshot>& mailbox);
}
