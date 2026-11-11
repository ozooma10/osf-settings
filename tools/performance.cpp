#include "API/SettingsApi.h"
#include "../sdk/OSFSettingsRegistry.h"
#include "Settings/SettingsService.h"
#include "Settings/Localization.h"
#include "Settings/SettingsJson.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <nlohmann/json.hpp>

// Count C++ allocations on the measured host thread in a separate, untimed pass.
// This does not measure the engine allocator, other threads, or live heap size.
namespace { thread_local bool counting{}; thread_local std::size_t allocations{}, bytes{}; }
void* operator new(std::size_t size) {
    if (auto* p = std::malloc(size ? size : 1)) {
        if (counting) { ++allocations; bytes += size; }
        return p;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
    using namespace OSFSettings;
    using Json = nlohmann::json;
    using Clock = std::chrono::steady_clock;
    std::size_t observed{};
    template<class F> Json Measure(F fn, int samples = 41, int batch = 1) {
        std::vector<double> times;
        times.reserve(samples);
        const auto first = Clock::now(); fn();
        const double firstUs = std::chrono::duration<double, std::micro>(Clock::now() - first).count();
        for (int i = 0; i < 5; ++i) fn();
        for (int i = 0; i < samples; ++i) {
            const auto start = Clock::now();
            for (int j = 0; j < batch; ++j) fn();
            times.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count() / batch);
        }
        allocations = bytes = 0; counting = true; fn(); counting = false;
        const auto allocationCount = allocations, allocatedBytes = bytes;
        auto sorted = times; std::ranges::sort(sorted);
        return {{"samples_us", times}, {"first_us", firstUs}, {"batch", batch},
            {"p50_us", sorted[sorted.size()/2]}, {"p95_us", sorted[(sorted.size()-1)*95/100]},
            {"max_us", sorted.back()}, {"mean_us", std::accumulate(times.begin(), times.end(), 0.0)/times.size()},
            {"allocations", allocationCount}, {"allocated_bytes", allocatedBytes}};
    }
    void ObserveRegistry(const API::RegistryView& view, void*) noexcept {
        for (std::uint32_t m = 0; m < view.modCount; ++m)
            for (std::uint32_t g = 0; g < view.mods[m].groupCount; ++g)
                observed += view.mods[m].groups[g].settingCount;
    }
}

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("usage: osfsettings-performance FIXTURE_DIRECTORY OUTPUT_JSON");
        const std::filesystem::path root = argv[1];
        Json result{{"protocol", "host single-thread; 5 warmups; 41 samples; destruction included; allocations separate; OS cache uncontrolled"}};
        SettingsService service;
        result["load"] = Measure([&] { SettingsStore store; store.LoadAll(root / "schemas", root / "values"); observed += store.Mods().size(); }, 11);
        service.Load(root / "schemas", root / "values");
        if (!service.Start()) throw std::runtime_error("service failed to start");
        if (!service.LoadErrors().empty()) throw std::runtime_error("fixture load errors");
        auto source = service.Snapshot();
        std::vector<std::pair<std::string,std::string>> keys;
        for (const auto& mod : source) for (const auto& [key, value] : mod.values) keys.emplace_back(mod.schema.id, key);
        result["mods"] = source.size(); result["settings"] = keys.size();
        result["localize"] = Measure([&] { service.Localize(root / "translations", "ja"); }, 11);
        result["snapshot"] = Measure([&] { auto copy = service.Snapshot(); observed += copy.size(); });
        if (!source.empty()) {
            result["find_mod"] = Measure([&] { auto copy = service.FindMod(source.back().schema.id); observed += copy->values.size(); });
            result["overlay"] = Measure([&] { auto copy = source.back().schema; Localization::Get()->Apply(copy); observed += copy.groups.size(); });
        }
        result["read_all"] = Measure([&] { for (const auto& [mod, key] : keys) { if (!service.GetValue(mod, key)) throw std::runtime_error("missing value"); ++observed; } });
        API::SettingsApi api(service);
        result["registry"] = Measure([&] { if (api.ReadRegistry(nullptr, ObserveRegistry, nullptr) != API::Status::Ok) throw std::runtime_error("registry read failed"); });
        result["idle_pending"] = Measure([&] { observed += service.HasPendingChanges(); }, 41, 10000);
        // Full-value providers and repeated replacement use the production ownership/token path.
        SettingsService providers; providers.Load(root / "empty", root / "unused"); providers.Start();
        std::vector<std::uint64_t> tokens(source.size());
        for (std::size_t i = 0; i < source.size(); ++i)
            if (providers.RegisterProvider(source[i], [](const SettingValues&) { return true; }, tokens[i]) != SettingsError::None)
                throw std::runtime_error("provider registration failed");
        result["provider_snapshot"] = Measure([&] { auto copy = providers.Snapshot(); observed += copy.size(); });
        if (!source.empty()) result["provider_replace"] = Measure([&] {
            if (providers.RegisterProvider(source.back(), [](const SettingValues&) { return true; }, tokens.back()) != SettingsError::None)
                throw std::runtime_error("provider replacement failed");
        });
        for (auto token : tokens) if (providers.UnregisterProvider(token) != SettingsError::None) throw std::runtime_error("provider cleanup failed");
        result["observed"] = observed;
        std::ofstream(argv[2]) << result.dump(2) << '\n';
        std::cout << result.at("mods") << " mods, " << result.at("settings") << " settings; snapshot p50 " << result["snapshot"]["p50_us"] << " us\n";
    } catch (const std::exception& error) { counting = false; std::cerr << error.what() << '\n'; return 1; }
}
