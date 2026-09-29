#include "Launcher/LauncherService.h"
#include "OSFSettings_Launcher.h"
#include "Settings/SettingsSchema.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

namespace OSFSettings
{
    std::optional<std::uint32_t> KeyCodeFromName(std::string_view) { throw std::runtime_error("unexpected key lookup"); }
    bool IsBindableKey(std::uint32_t) { throw std::runtime_error("unexpected key validation"); }
}
extern "C" void* OSFSettings_RequestLauncherAPI(std::uint32_t, std::uint32_t*) noexcept;

int main()
{
    using namespace OSFSettings;
    unsigned checks{};
    const auto check = [&](bool value, const char* message) { if (!value) throw std::runtime_error(message); ++checks; };
    try {
        LauncherService service;
        LaunchDestination native{ "demo", "native", "Demo", "Open native", "Native menu", "DemoMenu", {} };
        check(service.Register(native) == LauncherError::None, "menu-only mod registers without settings");
        check(service.Register(native) == LauncherError::AlreadyRegistered, "duplicate identity rejected");
        auto invalid = native; invalid.id = "bad"; invalid.menu.clear();
        check(service.Register(invalid) == LauncherError::InvalidArgument, "target required");
        invalid.menu = "OSFSettingsMenu";
        check(service.Register(invalid) == LauncherError::InvalidArgument, "recursive hub target rejected");
        invalid.menu = "DemoMenu"; invalid.open = [](const auto&, const auto&, std::uint64_t) {};
        check(service.Register(invalid) == LauncherError::InvalidArgument, "ambiguous target rejected");
        invalid = native; invalid.id = "bad"; invalid.title = std::string("a\0b", 3);
        check(service.Register(invalid) == LauncherError::InvalidArgument, "invalid display strings rejected");
        invalid = native; invalid.mod = "Bad Mod";
        check(service.Register(invalid) == LauncherError::InvalidArgument, "invalid mod identity rejected");
        invalid = native; invalid.id = "bad"; invalid.reason = std::string(4097, 'x');
        check(service.Register(invalid) == LauncherError::InvalidArgument, "oversized reason rejected");
        auto copy = service.Find("demo", "native"); copy->title = "changed";
        check(service.Snapshot()[0].title == native.title, "lookup returns owned metadata");
        check(!service.Find("demo", "missing"), "unknown destination not found");
        const auto revision = service.Revision();
        check(service.SetAvailable("demo", "native", false, "Disabled") == LauncherError::None, "availability updated");
        const auto disabled = service.Find("demo", "native");
        check(disabled && !disabled->available && disabled->reason == "Disabled", "disabled entries retain reason");
        check(service.Revision() > revision, "availability refreshes menu revision");
        check(service.SetAvailable("demo", "missing", true, "") == LauncherError::NotFound, "unknown availability rejected");
        check(service.SetAvailable("demo", "native", true, std::string("a\0b", 3)) == LauncherError::InvalidArgument, "invalid availability reason rejected");
        check(!service.Find("demo", "native")->available, "invalid update preserves availability");

        ModSettings mod;
        mod.schema.id = "absolute-control"; mod.schema.title = "Absolute Control";
        mod.schema.menus.push_back({ "panel", "Absolute Control", "Configure modules.", "AbsoluteControlPanelMenu" });
        service.Initialize({ mod });
        const auto declared = service.Find("absolute-control", "panel");
        check(declared && declared->menu == "AbsoluteControlPanelMenu" && declared->modTitle == mod.schema.title &&
            declared->title == "Absolute Control" && declared->description == "Configure modules.", "schema menu registration retains owner and destination metadata");
        service.Initialize({ mod });
        check(service.Snapshot().size() == 2, "schema initialization cannot duplicate entries");
        mod.schema.menus[0].title = "Changed";
        check(service.Find("absolute-control", "panel")->title == "Absolute Control", "schema registrations own their metadata");

        const auto directory = std::filesystem::temp_directory_path() /
            ("osf-launcher-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto history = directory / "internal.json";
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
        } cleanup{directory};
        const auto read = [](const std::filesystem::path& path) { std::ifstream input(path); return nlohmann::json::parse(input); };
        const auto values = directory / "values" / "osfsettings.json";
        std::filesystem::create_directories(values.parent_path());
        const nlohmann::json settings = {{"formatVersion", 1}, {"values", {{"enabled", false}}}};
        { std::ofstream output(values); output << settings; }
        const auto rank = [](const LauncherService& source, std::string_view mod) {
            for (const auto& entry : source.Snapshot()) if (entry.mod == mod) return entry.recentOrder;
            return std::uint32_t{};
        };
        service.LoadHistory(directory);
        const auto unchanged = service.Revision();
        check(!service.RecordOpened("demo", "missing") && !service.RecordOpened("demo", "native"), "missing and unavailable destinations cannot enter recent history");
        check(service.Revision() == unchanged && !std::filesystem::exists(history), "rejected openings do not change or write history");
        check(service.RecordOpened("absolute-control", "panel") && rank(service, "absolute-control") > rank(service, "demo"), "opened interface ranks before unopened interfaces");
        service.SetAvailable("demo", "native", true, "");
        check(service.RecordOpened("demo", "native") && rank(service, "demo") > rank(service, "absolute-control"), "last opening moves the destination to the front");
        check(service.RecordOpened("absolute-control", "panel") && rank(service, "absolute-control") == 2 && rank(service, "demo") == 1, "repeated openings reorder without duplicate history entries");
        check(read(history).size() == 1 && read(history)["recentLaunchers"][0]["mod"] == "absolute-control" &&
            read(values) == settings, "internal data uses its fixed field and preserves mod settings files");
        LauncherService restored;
        restored.LoadHistory(directory); // Providers may register after history is loaded.
        restored.Register(native); restored.Initialize({mod});
        check(rank(restored, "absolute-control") == 2 && rank(restored, "demo") == 1, "recent ordering survives a new service and late provider registration");
        std::filesystem::create_directory(directory / "internal.json.tmp");
        const auto restoredRevision = restored.Revision();
        check(restored.RecordOpened("absolute-control", "panel") && restored.Revision() == restoredRevision,
            "opening the front destination leaves unchanged history alone");
        check(restored.RecordOpened("demo", "native") && rank(restored, "demo") == 2 &&
            read(history)["recentLaunchers"][0]["mod"] == "absolute-control",
            "failed save preserves disk history and updates session recency");
        std::filesystem::remove(directory / "internal.json.tmp");
        check(restored.RecordOpened("demo", "native") && read(history)["recentLaunchers"][0]["mod"] == "demo",
            "reopening the front destination retries a failed save");
        { std::ofstream broken(history); broken << "{broken"; }
        restored.LoadHistory(directory);
        check(restored.Snapshot().size() == 2 && rank(restored, "demo") == 0, "malformed history preserves registered interfaces with default ordering");
        restored.LoadHistory(history);
        check(restored.RecordOpened("demo", "native") && rank(restored, "demo") > 0, "history write failure preserves in-memory recency and does not reject opening");

        namespace API = OSFSettings::API::Launcher;
        std::uint32_t version = 9;
        check(!OSFSettings_RequestLauncherAPI(0, &version) && version == 0, "unversioned ABI rejected");
        check(!OSFSettings_RequestLauncherAPI(0x20000, &version) && version == 0, "prepare/cancel ABI rejected");
        check(!OSFSettings_RequestLauncherAPI(API::kVersion + 1, &version) && version == 0, "future ABI rejected");
        auto* api = static_cast<API::ILauncher*>(OSFSettings_RequestLauncherAPI(API::kVersion, &version));
        check(api && version == API::kVersion, "independent service export");
        check(api->Register({}) == API::Status::InvalidArgument, "SDK validates null metadata");
        std::string title = "API menu";
        check(api->Register({ .modId = "api", .id = "menu", .title = title.c_str(), .menu = "ApiMenu" }) == API::Status::Ok, "SDK native registration");
        title = "changed";
        check(LauncherService::Get().Find("api", "menu")->title == "API menu", "SDK metadata copied at registration");
        check(api->SetAvailable("api", "menu", false, "Disabled") == API::Status::Ok, "SDK changes availability");
        struct Context {
            API::ILauncher* api;
            int requested{}, opened{};
            std::uint64_t request{};
            bool identity{};
            API::Status result{};
        } context{api};
        const API::OpenFn afterClose = [](const char* mod, const char* id, std::uint64_t request, void* state) noexcept {
            auto& value = *static_cast<Context*>(state);
            ++value.opened;
            value.request = request;
            value.identity = std::string_view(mod) == "api" && std::string_view(id) == "web/view";
            value.result = value.api->SetAvailable(mod, id, true, "");
        };
        API::Destination target{
            .modId = "api", .id = "web/view", .title = "View",
            .open = [](const char* mod, const char* id, std::uint64_t request, void* state) noexcept {
                auto& value = *static_cast<Context*>(state);
                ++value.requested;
                value.request = request;
                value.identity = std::string_view(mod) == "api" && std::string_view(id) == "web/view";
                value.result = value.api->SetAvailable(mod, id, true, "");
            }, .context = &context
        };
        auto invalidTarget = target; invalidTarget.menu = "ApiMenu";
        check(api->Register(invalidTarget) == API::Status::InvalidArgument, "native and callback targets are mutually exclusive");
        check(api->Register(target) == API::Status::Ok, "one open callback registers a destination");
        auto& live = LauncherService::Get();
        const auto callback = live.Find("api", "web/view");
        check(!live.BeginOpen("api", "menu") && !live.BeginOpen("api", "missing"), "only available callback destinations get a request");
        check(api->Complete(0, afterClose, &context, "") == API::Status::InvalidArgument, "zero is not a request ID");
        check(api->Complete(999, afterClose, &context, "") == API::Status::NotFound && !live.TakeResult(999), "unknown completion is rejected");
        const auto first = live.BeginOpen(callback->mod, callback->id);
        check(first && !live.BeginOpen(callback->mod, callback->id), "one owned request; cannot silently replace it");
        callback->open(callback->mod, callback->id, first);
        check(context.requested == 1 && context.identity && context.request == first && context.result == API::Status::Ok,
            "open forwards identity and can call the API outside registry lock");
        check(!context.opened && !live.TakeResult(first), "returning from open does not imply readiness");
        const auto beforeReady = live.Revision();
        check(api->Complete(first, afterClose, &context, "") == API::Status::Ok, "provider completes with after-close callback");
        check(api->Complete(first, nullptr, nullptr, "duplicate") == API::Status::NotFound, "duplicate completion rejected");
        const auto completed = live.TakeResult(first);
        check(completed && completed->afterClose && completed->reason.empty(), "first completion wins");
        check(!live.TakeResult(first) && live.Revision() == beforeReady && !context.opened,
            "completion neither activates nor records recent history");
        check(api->Complete(first, afterClose, &context, "duplicate after poll") == API::Status::NotFound, "consuming result cannot admit another completion");
        live.EndOpen(first); // Settings removal invalidates the request before dispatch.
        completed->afterClose(callback->mod, callback->id, first);
        check(context.opened == 1 && context.identity && context.request == first && context.result == API::Status::Ok,
            "after-close callback gets its own context and request, outside registry lock");
        check(live.RecordOpened(callback->mod, callback->id) && std::ranges::any_of(live.Snapshot(), [](const auto& value) { return value.id == "web/view" && value.recentOrder > 0; }), "history records the handoff");
        const auto afterOpen = live.Revision();
        check(api->Complete(first, afterClose, &context, "") == API::Status::NotFound && !live.TakeResult(first) && live.Revision() == afterOpen,
            "late completion after handoff is rejected without changing history");

        const auto failed = live.BeginOpen(callback->mod, callback->id);
        check(api->Complete(failed, nullptr, nullptr, std::string(4097, 'x').c_str()) == API::Status::InvalidArgument && !live.TakeResult(failed),
            "oversized reason leaves request pending");
        check(api->Complete(failed, nullptr, nullptr, "Page failed") == API::Status::Ok, "null callback reports failure");
        const auto failure = live.TakeResult(failed);
        check(failure && !failure->afterClose && failure->reason == "Page failed", "failure stays in Settings");
        live.EndOpen(failed);

        for (bool alreadyReady : {false, true}) {
            const auto abandoned = live.BeginOpen(callback->mod, callback->id);
            if (alreadyReady) check(api->Complete(abandoned, afterClose, &context, "") == API::Status::Ok, "completion may precede Back");
            live.EndOpen(abandoned); // Back/timeout invalidates without a provider cancellation callback.
            check(api->Complete(abandoned, afterClose, &context, "late") == API::Status::NotFound && !live.TakeResult(abandoned) && context.opened == 1,
                "Back discards both loading and unconsumed completion without activation");
            const auto next = live.BeginOpen(callback->mod, callback->id);
            check(next > abandoned, "reopening has a fresh identity");
            live.EndOpen(abandoned);
            check(api->Complete(abandoned, afterClose, &context, "stale") == API::Status::NotFound && !live.TakeResult(next) && !live.BeginOpen(callback->mod, callback->id),
                "old completion or abandonment cannot affect new wait");
            api->Complete(next, nullptr, nullptr, "new failure");
            check(live.TakeResult(next)->reason == "new failure", "new request receives its own result");
            live.EndOpen(next);
        }

        // A provider with nothing to load can complete synchronously in its one open callback.
        auto immediate = target; immediate.id = "immediate";
        immediate.open = [](const char*, const char*, std::uint64_t request, void* state) noexcept {
            auto& value = *static_cast<Context*>(state);
            value.result = value.api->Complete(request,
                [](const char*, const char*, std::uint64_t, void* context) noexcept { ++static_cast<Context*>(context)->opened; }, state, "");
        };
        check(api->Register(immediate) == API::Status::Ok, "immediate provider uses the same contract");
        const auto immediateRequest = live.BeginOpen("api", "immediate");
        live.Find("api", "immediate")->open("api", "immediate", immediateRequest);
        const auto immediateResult = live.TakeResult(immediateRequest);
        check(context.result == API::Status::Ok && immediateResult && immediateResult->afterClose && context.opened == 1,
            "synchronous completion is safe and still defers activation");
        live.EndOpen(immediateRequest);
        immediateResult->afterClose("api", "immediate", immediateRequest);
        check(context.opened == 2, "immediate provider activates only after removal");
        api->SetAvailable("api", "web/view", false, "Disabled");
        check(!live.BeginOpen("api", "web/view"), "unavailable destination cannot open");
        std::cout << checks << " launcher checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
