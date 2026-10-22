#pragma once

#ifdef OSFSETTINGS_TEST_HARNESS
#include <chrono>
#endif

namespace OSFSettings::TestHarness
{
    enum class PerformancePoint { Load, Localize, Snapshot, FindMod, GetValue, Dispatch, Compatibility, GetRows };
    // Inclusive wall time on the calling lane, not rendered-frame time or CPU time.
    // Fixed aggregation lives only in the test DLL; production scopes compile away.
    class PerformanceScope
    {
    public:
#ifdef OSFSETTINGS_TEST_HARNESS
        explicit PerformanceScope(PerformancePoint point) : m_point(point), m_start(std::chrono::steady_clock::now()) {}
        ~PerformanceScope();
    private:
        PerformancePoint m_point;
        std::chrono::steady_clock::time_point m_start;
#else
        explicit PerformanceScope(PerformancePoint) {}
#endif
    };
}
