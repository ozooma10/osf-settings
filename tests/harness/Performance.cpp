#include "Harness/Performance.h"
#include <Windows.h>
#include <array>
#include <mutex>
#include <nlohmann/json.hpp>

namespace OSFSettings::TestHarness
{
    namespace
    {
        struct Sample {
            DWORD thread{};
            PerformancePoint point{};
            std::uint64_t count{}, totalNs{}, maxNs{};
            std::array<std::uint64_t, 32> bins{};
        };
        std::mutex mutex;
        std::array<Sample, 128> samples;
        std::uint64_t dropped{};
        constexpr const char* names[]{ "load", "localize", "snapshot", "find_mod", "get_value", "dispatch", "compatibility", "get_rows" };
    }

    PerformanceScope::~PerformanceScope()
    {
        const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - m_start).count());
        const auto thread = ::GetCurrentThreadId();
        std::scoped_lock lock(mutex);
        for (auto& sample : samples) {
            if (!sample.count || (sample.thread == thread && sample.point == m_point)) {
                sample.thread = thread; sample.point = m_point;
                ++sample.count; sample.totalNs += elapsed;
                sample.maxNs = (std::max)(sample.maxNs, elapsed);
                std::size_t bin{};
                while (bin + 1 < sample.bins.size() && elapsed > (std::uint64_t{64} << bin)) ++bin;
                ++sample.bins[bin];
                return;
            }
        }
        ++dropped;
    }

    nlohmann::json PerformanceSnapshot()
    {
        std::array<Sample, 128> copy;
        std::uint64_t lost;
        { std::scoped_lock lock(mutex); copy = samples; lost = dropped; }
        auto rows = nlohmann::json::array();
        for (const auto& sample : copy) {
            if (!sample.count) continue;
            rows.push_back({{"name", names[static_cast<unsigned>(sample.point)]}, {"thread", sample.thread},
                {"count", sample.count}, {"total_ns", sample.totalNs}, {"max_ns", sample.maxNs}, {"bins", sample.bins}});
        }
        return {{"rows", rows}, {"dropped", lost}, {"histogram", "bin i upper bound 64*2^i ns; last bin overflow"},
            {"clock", "steady_clock inclusive wall; aggregate after stop; nested scopes must not be added"}};
    }
}
