#include "Diagnostics/DiagnosticsService.h"

#include <atomic>
#include <barrier>
#include <set>
#include <stdexcept>
#include <thread>
#include <utility>

namespace
{
    OSFSettings::ModIssue Issue(std::string modId, const std::string& value)
    {
        return { .modId = std::move(modId), .id = "status", .title = value, .impact = value, .nextSteps = value };
    }
}

int TestDiagnosticsService()
{
    using namespace OSFSettings;
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };

    DiagnosticsService service;
    check(service.Snapshot().empty(), "a new diagnostics service is empty");
    auto input = Issue("sample", "Original");
    check(service.Report(input), "reporting works without settings initialization");
    input.title = "Caller edit";
    auto snapshot = service.Snapshot();
    check(snapshot.size() == 1 && snapshot[0].title == "Original", "the service owns reported strings");
    snapshot[0].title = "Snapshot edit";
    check(service.Snapshot()[0].title == "Original", "service snapshots own their strings");
    check(!service.Report(Issue("sample", "")) && service.Snapshot()[0].title == "Original",
        "invalid reports preserve the existing issue");
    check(service.Report(Issue("sample", "Updated")) && service.Snapshot().size() == 1 &&
        service.Snapshot()[0].title == "Updated", "service reports update the same identity");
    check(service.Clear("sample", "status") && !service.Clear("sample", "status"),
        "service clears remove issues and tolerate repetition");

    check(service.Report(Issue("sentinel", "Sentinel")), "unrelated issue is present before concurrent operations");
    constexpr int workerCount = 4;
    std::barrier start(workerCount + 1);
    std::atomic_int finished{};
    std::atomic_bool accepted{ true };
    std::vector<std::jthread> workers;
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&, worker] {
            const auto modId = "worker" + std::to_string(worker);
            start.arrive_and_wait();
            for (int iteration = 0; iteration < 128; ++iteration) {
                const auto value = modId + ":" + std::to_string(iteration);
                if (!service.Report(Issue(modId, value)) || !service.Report(Issue("shared", value))) accepted = false;
                if (iteration % 7 == 0) service.Clear("shared", "status");
                if (iteration % 11 == 0) service.ClearMod(modId);
            }
            const auto finalValue = modId + ":done";
            if (!service.Report(Issue(modId, finalValue)) || !service.Report(Issue("shared", finalValue))) accepted = false;
            ++finished;
        });
    }

    start.arrive_and_wait();
    bool coherent = true;
    do {
        std::set<std::pair<std::string, std::string>> identities;
        for (const auto& issue : service.Snapshot()) {
            coherent = coherent && issue.title == issue.impact && issue.impact == issue.nextSteps &&
                identities.emplace(issue.modId, issue.id).second;
        }
    } while (finished < workerCount);
    workers.clear(); // Join before inspecting the final state or reporting failures.

    check(accepted, "concurrent valid reports are accepted");
    check(coherent, "concurrent snapshots contain whole reports with no duplicate identities");
    const auto final = service.Snapshot();
    check(final.size() == workerCount + 2, "concurrent reports retain each worker, one shared issue, and the unrelated issue");
    for (const auto& issue : final) {
        check(issue.modId == "sentinel" ? issue.title == "Sentinel" :
            issue.modId == "shared" ? issue.title.ends_with(":done") : issue.title == issue.modId + ":done",
            "concurrent updates finish with the expected complete report");
    }
    check(service.ClearMod("shared") == 1 && service.ClearMod("shared") == 0,
        "clearing a mod removes the one shared issue without retaining history");
    check(service.Snapshot().size() == workerCount + 1, "clearing a mod preserves other mods");
    check(&DiagnosticsService::Get() == &DiagnosticsService::Get(), "the process service has a stable identity");
    return checks;
}
