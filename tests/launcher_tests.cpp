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
        check(!OSFSettings_RequestLauncherAPI(0x20000, &version) && version == 0, "different major ABI rejected");
        check(!OSFSettings_RequestLauncherAPI(API::kVersion + 1, &version) && version == 0, "future ABI rejected");
        auto* api = static_cast<API::ILauncher*>(OSFSettings_RequestLauncherAPI(API::kVersion, &version));
        check(api && version == API::kVersion, "independent service export");
        check(api->Register({}) == API::Status::InvalidArgument, "SDK validates null metadata");
        std::string title = "API menu";
        check(api->Register({ .modId = "api", .id = "menu", .title = title.c_str(), .menu = "ApiMenu" }) == API::Status::Ok, "SDK native registration");
        title = "changed";
        check(LauncherService::Get().Find("api", "menu")->title == "API menu", "SDK metadata copied at registration");
        check(api->SetAvailable("api", "menu", false, "Disabled") == API::Status::Ok, "SDK changes availability");
        struct Context { API::ILauncher* api; bool called{}; std::uint64_t request{}; API::Status result{}; } context{ api };
        check(api->Register({ .modId = "api", .id = "web/view", .title = "View",
            .open = [](const char* mod, const char* id, std::uint64_t request, void* state) noexcept {
                auto& target = *static_cast<Context*>(state);
                target.called = std::string_view(mod) == "api" && std::string_view(id) == "web/view";
                target.request = request;
                target.result = target.api->SetAvailable(mod, id, false, "Opening");
            }, .context = &context }) == API::Status::Ok, "SDK callback registration");
        auto& live = LauncherService::Get();
        const auto callback = live.Find("api", "web/view");
        const auto deferred = live.BeginOpen(callback->mod, callback->id);
        check(deferred != 0, "callback gets a request before it starts");
        callback->open(callback->mod, callback->id, deferred);
        check(context.called && context.request == deferred && context.result == API::Status::Ok, "copied callback forwards identity/request/context without registry lock");
        check(!live.Find("api", "web/view")->available, "provider can update its own availability");
        check(!live.TakeReport(deferred) && live.Find("api", "web/view")->recentOrder == 0, "returning from open does not complete the request or record history");
        check(api->ReportOpened(deferred, false, "Deferred failure") == API::Status::Ok, "provider can report after its callback returns");
        const auto deferredResult = live.TakeReport(deferred);
        check(deferredResult && !deferredResult->opened && deferredResult->reason == "Deferred failure", "explicit report completes deferred callback");

        // One asynchronous open, followed by an identified terminal result. The
        // callback may report immediately without running under the registry lock.
        struct AsyncContext { API::ILauncher* api; unsigned calls{}; std::uint64_t request{}; bool identity{}; API::Status result{}; } async{api};
        API::Destination asyncDestination{ .modId = "api", .id = "async/view", .title = "Async view",
            .open = [](const char* mod, const char* id, std::uint64_t request, void* state) noexcept {
                auto& value = *static_cast<AsyncContext*>(state);
                ++value.calls;
                value.request = request;
                value.identity = std::string_view(mod) == "api" && std::string_view(id) == "async/view";
                value.result = value.api->ReportOpened(request, true, "");
            }, .context = &async };
        auto ambiguous = asyncDestination;
        ambiguous.menu = "ApiMenu";
        check(api->Register(ambiguous) == API::Status::InvalidArgument, "native and async targets are mutually exclusive");
        check(api->Register(asyncDestination) == API::Status::Ok, "SDK asynchronous registration");
        check(!live.BeginOpen("api", "menu") && !live.BeginOpen("api", "web/view") && !live.BeginOpen("api", "missing"), "only available asynchronous destinations start waits");
        check(api->ReportOpened(0, true, "") == API::Status::InvalidArgument, "zero is not a request ID");
        check(api->ReportOpened(999, true, "") == API::Status::Ok && !live.TakeReport(999), "unknown results cannot create a wait");
        const auto first = live.BeginOpen("api", "async/view");
        check(first && !live.TakeReport(first), "wait exists before invoking provider");
        const auto destination = live.Find("api", "async/view");
        destination->open(destination->mod, destination->id, first);
        check(async.calls == 1 && async.identity && async.request == first && async.result == API::Status::Ok, "copied async callback forwards identity/request/context and can report synchronously");
        check(std::ranges::any_of(live.Snapshot(), [](const auto& value) { return value.id == "async/view" && value.recentOrder > 0; }), "success records history without a waiting menu polling");
        const auto successRevision = live.Revision();
        check(api->ReportOpened(first, false, "Late failure") == API::Status::Ok, "duplicate result ignored");
        const auto success = live.TakeReport(first);
        check(success && success->opened && success->reason.empty(), "first terminal result wins");
        check(!live.TakeReport(first) && async.calls == 1, "consuming success neither reopens nor repeats result");
        api->ReportOpened(first, true, "");
        check(!live.TakeReport(first) && live.Revision() == successRevision, "consumed completion remains terminal");
        const auto second = live.BeginOpen("api", "async/view");
        check(second > first, "same destination gets a fresh request ID");
        api->ReportOpened(first, true, "");
        check(!live.TakeReport(second), "stale success cannot complete a reopened menu's request");
        check(api->ReportOpened(second, false, std::string(4097, 'x').c_str()) == API::Status::InvalidArgument && !live.TakeReport(second), "oversized reason leaves request pending");
        check(api->ReportOpened(second, false, "Page failed") == API::Status::Ok, "provider reports failure");
        check(!live.TakeReport(first), "an older menu cannot consume another request's result");
        const auto failure = live.TakeReport(second);
        check(failure && !failure->opened && failure->reason == "Page failed" && live.Revision() == successRevision, "failure keeps its reason without recording an opening");
        const auto abandoned = live.BeginOpen("api", "async/view");
        const auto replacement = live.BeginOpen("api", "async/view");
        api->ReportOpened(abandoned, true, "");
        check(!live.TakeReport(replacement), "superseded pending request cannot complete replacement");
        api->SetAvailable("api", "async/view", false, "Disabled");
        check(!live.BeginOpen("api", "async/view"), "unavailable asynchronous destination is rejected");
        std::cout << checks << " launcher checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
