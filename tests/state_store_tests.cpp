#include "Settings/StateStore.h"
#include "Settings/SettingsStore.h"
#include "Launcher/LauncherService.h"

#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>

namespace OSFSettings
{
    std::optional<std::uint32_t> KeyCodeFromName(std::string_view) { throw std::runtime_error("unexpected key lookup"); }
    bool IsBindableKey(std::uint32_t) { throw std::runtime_error("unexpected key validation"); }
}

int main()
{
    namespace fs = std::filesystem;
    using namespace OSFSettings;
    using Json = nlohmann::json;
    const auto root = fs::temp_directory_path() / ("osf-state-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { fs::path root; ~Cleanup() { std::error_code ignored; fs::remove_all(root, ignored); } } cleanup{root};
    unsigned checks{};
    const auto check = [&](bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); };
    const auto write = [](const fs::path& path, const Json& document) { fs::create_directories(path.parent_path()); std::ofstream(path) << document; };
    const auto read = [](const fs::path& path) { std::ifstream file(path); return Json::parse(file); };
    try {
        const auto values = root / "values", schemas = root / "schemas", file = values / "osfsettings.json";
        const auto oldHistory = root / "launcher-history.json";
        const Json original = {{"formatVersion", 1}, {"values", {{"enabled", false}, {"unknown", 17}}}};
        write(file, original);
        write(values / "demo.json", original);
        write(values / "disabled-mod.json", original);
        const Json history = {{"formatVersion", 1}, {"recent", {{{"mod", "demo"}, {"id", "panel"}}}}};
        write(oldHistory, history);
        for (const auto* mod : {"osfsettings", "demo"}) {
            write(schemas / (std::string(mod) + ".json"), {{"groups", {{"General", {
                {{"key", "enabled"}, {"type", "bool"}, {"default", true}}
            }}}}});
        }
        auto state = std::make_shared<StateStore>(file, oldHistory);
        check(state->LoadErrors().empty() && read(file) == original, "history import does not rewrite existing values on load");
        SettingsStore settings;
        settings.LoadAll(schemas, values, state);
        LauncherService launcher;
        launcher.LoadHistory(state);
        launcher.Register({"demo", "panel", "Demo", "Panel", "", "DemoMenu", {}});
        launcher.Register({"demo", "other", "Demo", "Other", "", "OtherMenu", {}});
        check(settings.GetValue("osfsettings", "enabled") == SettingValue{false} &&
            settings.GetValue("demo", "enabled") == SettingValue{false} && launcher.Snapshot()[0].recentOrder == 1,
            "per-mod values and legacy history load together");
        check(settings.Set("demo", "enabled", true).ok && read(values / "demo.json")["values"]["enabled"] == true &&
            read(file) == original, "editing another mod writes only its own file");
        check(settings.Set("osfsettings", "enabled", true).ok, "OSF setting edit persists imported history");
        check(read(file)["launcher"] == history && read(file)["values"]["unknown"] == 17 && !read(file).contains("settings"),
            "OSF file keeps the existing values shape alongside history and unknown keys");
        check(read(values / "disabled-mod.json") == original && !fs::exists(root / "state.json"),
            "disabled mods remain untouched and no combined global file is created");
        check(launcher.RecordOpened("demo", "other") && read(file)["values"]["enabled"] == true &&
            read(values / "demo.json")["values"]["enabled"] == true, "history updates preserve all mod values");
        check(settings.Set("osfsettings", "enabled", false).ok && read(file)["launcher"]["recent"][0]["id"] == "other",
            "OSF value update preserves the latest history");
        check(read(oldHistory) == history, "legacy history remains available for rollback");
        StateStore restarted(file, oldHistory);
        check(restarted.Read("launcher")["recent"][0]["id"] == "other", "embedded history takes precedence after restart");
        const auto revision = launcher.Revision();
        fs::create_directory(values / "osfsettings.json.tmp");
        check(launcher.RecordOpened("demo", "other") && launcher.Revision() == revision,
            "opening the front destination does not rewrite unchanged history");
        check(launcher.RecordOpened("demo", "panel") && read(file)["launcher"]["recent"][0]["id"] == "other",
            "failed history save preserves disk and allows session recency");
        check(!settings.Set("osfsettings", "enabled", true).ok && settings.GetValue("osfsettings", "enabled") == SettingValue{false},
            "failed OSF save preserves published values");
        check(settings.Set("demo", "enabled", false).ok && read(values / "demo.json")["values"]["enabled"] == false,
            "an unwritable OSF file does not block another mod's settings");
        fs::remove(values / "osfsettings.json.tmp");
        check(launcher.RecordOpened("demo", "panel") && read(file)["launcher"]["recent"][0]["id"] == "panel",
            "reopening unchanged history retries an unsuccessful save");
        auto a = std::async(std::launch::async, [&] { for (int i = 0; i < 12; ++i) if (!settings.Set("osfsettings", "enabled", i % 2 == 0).ok) return false; return true; });
        auto b = std::async(std::launch::async, [&] { for (int i = 0; i < 12; ++i) if (!launcher.RecordOpened("demo", i % 2 == 0 ? "panel" : "other")) return false; return true; });
        check(a.get() && b.get() && read(file)["values"]["enabled"] == false &&
            read(file)["launcher"]["recent"][0]["id"] == "other", "concurrent OSF values and history writes preserve both sections");
        auto future = read(file); future["formatVersion"] = 2; write(file, future);
        StateStore newer(file, oldHistory);
        std::string error;
        check(!newer.LoadErrors().empty() && !newer.Write("launcher", history, error) && read(file) == future,
            "newer OSF values versions are preserved");
        settings.LoadAll(schemas, values);
        check(settings.Set("demo", "enabled", true).ok && read(file) == future,
            "invalid OSF state does not prevent other mods from saving");
        std::cout << checks << " per-mod state checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
