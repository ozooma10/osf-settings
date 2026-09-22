#include "Launcher/LauncherService.h"
#include "OSFSettings_Launcher.h"
#include "Settings/SettingsSchema.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <fstream>

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
        invalid.menu = "DemoMenu"; invalid.open = [](const auto&, const auto&) {};
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

        const auto history = std::filesystem::temp_directory_path() /
            ("osf-launcher-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); path += ".tmp"; std::filesystem::remove(path, ignored); }
        } cleanup{history};
        const auto rank = [](const LauncherService& source, std::string_view mod) {
            for (const auto& entry : source.Snapshot()) if (entry.mod == mod) return entry.recentOrder;
            return std::uint32_t{};
        };
        service.LoadHistory(history);
        const auto unchanged = service.Revision();
        check(!service.RecordOpened("demo", "missing") && !service.RecordOpened("demo", "native"), "missing and unavailable destinations cannot enter recent history");
        check(service.Revision() == unchanged && !std::filesystem::exists(history), "rejected openings do not change or write history");
        check(service.RecordOpened("absolute-control", "panel") && rank(service, "absolute-control") > rank(service, "demo"), "opened interface ranks before unopened interfaces");
        service.SetAvailable("demo", "native", true, "");
        check(service.RecordOpened("demo", "native") && rank(service, "demo") > rank(service, "absolute-control"), "last opening moves the destination to the front");
        check(service.RecordOpened("absolute-control", "panel") && rank(service, "absolute-control") == 2 && rank(service, "demo") == 1, "repeated openings reorder without duplicate history entries");
        LauncherService restored;
        restored.LoadHistory(history); // Providers may register after history is loaded.
        restored.Register(native); restored.Initialize({mod});
        check(rank(restored, "absolute-control") == 2 && rank(restored, "demo") == 1, "recent ordering survives a new service and late provider registration");
        { std::ofstream broken(history); broken << "{broken"; }
        restored.LoadHistory(history);
        check(restored.Snapshot().size() == 2 && rank(restored, "demo") == 0, "malformed history preserves registered interfaces with default ordering");
        restored.LoadHistory(history / "unwritable.json");
        check(restored.RecordOpened("demo", "native") && rank(restored, "demo") > 0, "history write failure preserves in-memory recency and does not reject opening");

        namespace API = OSFSettings::API::Launcher;
        std::uint32_t version = 9;
        check(!OSFSettings_RequestLauncherAPI(0x10000, &version) && version == 0, "previous session ABI rejected");
        check(!OSFSettings_RequestLauncherAPI(API::kVersion + 1, &version) && version == 0, "future ABI rejected");
        auto* api = static_cast<API::ILauncher*>(OSFSettings_RequestLauncherAPI(API::kVersion, &version));
        check(api && version == API::kVersion, "independent service export");
        check(api->Register({}) == API::Status::InvalidArgument, "SDK validates null metadata");
        std::string title = "API menu";
        check(api->Register({ .modId = "api", .id = "menu", .title = title.c_str(), .menu = "ApiMenu" }) == API::Status::Ok, "SDK native registration");
        title = "changed";
        check(LauncherService::Get().Find("api", "menu")->title == "API menu", "SDK metadata copied at registration");
        check(api->SetAvailable("api", "menu", false, "Disabled") == API::Status::Ok, "SDK changes availability");
        struct Context { API::ILauncher* api; bool called{}; API::Status result{}; } context{ api };
        check(api->Register({ .modId = "api", .id = "web/view", .title = "View",
            .open = [](const char* mod, const char* id, void* state) noexcept {
                auto& target = *static_cast<Context*>(state);
                target.called = std::string_view(mod) == "api" && std::string_view(id) == "web/view";
                target.result = target.api->SetAvailable(mod, id, false, "Opening");
            }, .context = &context }) == API::Status::Ok, "SDK callback registration");
        const auto callback = LauncherService::Get().Find("api", "web/view");
        callback->open(callback->mod, callback->id);
        check(context.called && context.result == API::Status::Ok, "copied callback forwards identity/context without registry lock");
        check(!LauncherService::Get().Find("api", "web/view")->available, "provider can update its own availability");
        std::cout << checks << " launcher checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
