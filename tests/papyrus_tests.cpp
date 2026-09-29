#include "Papyrus/Subscriptions.h"
#include "Papyrus/Values.h"
#include "Papyrus/Issues.h"
#include "Diagnostics/DiagnosticsService.h"

#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace
{
    using namespace OSFSettings;
    using namespace OSFSettings::Papyrus;
    using Kind = Subscriptions::Kind;
    using TestJson = nlohmann::ordered_json;
    constexpr auto Mod = "papyrusexample";
    const NativeHotkeys::Action toggle{ "papyrusexample/toggle", Mod, "toggle", std::nullopt };
}

int main()
{
    unsigned checks{};
    const auto check = [&](bool pass, const char* message) {
        if (!pass) throw std::runtime_error(message);
        ++checks;
    };
    try {
        const auto root = std::filesystem::temp_directory_path() /
            ("osfsettings-papyrus-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup
        {
            std::filesystem::path root;
            ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
        } cleanup{ root };
        const auto schemas = root / "schemas", values = root / "values";
        std::filesystem::create_directories(schemas);
        auto schema = TestJson::parse(std::ifstream("examples/papyrus/papyrusexample.json"));
        schema["groups"]["General"].push_back({{"key", "wideInt"}, {"type", "int"}, {"default", INT64_MAX}});
        schema["groups"]["General"].push_back({{"key", "wideFloat"}, {"type", "float"}, {"default", 1e100}});
        schema["groups"]["General"].push_back({{"key", "CaseKey"}, {"type", "bool"}, {"default", false}});
        schema["groups"]["General"].push_back({{"key", "casekey"}, {"type", "bool"}, {"default", false}});
        schema["hotkeys"]["menu"] = {{"label", "Menu"}, {"menu", "ExampleMenu"}};
        { std::ofstream file(schemas / "papyrusexample.json"); file << schema; }

        SettingsService settings;
        Values api(settings);
        HotkeyInputState input;
        std::vector<std::string> events;
        std::function<void()> onSubmit;
        const auto collect = [&](const Receiver& receiver, Kind kind, const std::string& mod, const std::string& key) {
            check(mod == Mod, "delivery owns the mod ID");
            events.push_back(receiver.script + ":" + (kind == Kind::Changes ? "change:" : "hotkey:") + key);
            if (onSubmit) onSubmit();
        };
        Subscriptions listeners(settings, input, collect);
        const Receiver instance{ 42, "example" }, global{ 0, "globalexample" };
        check(api.Read("GetInt", Mod, "count", std::int32_t(-7)) == -7, "unready reads use fallback");
        check(!api.Write("SetBool", Mod, "enabled", true), "unready writes fail");
        check(listeners.Register(instance, Kind::Changes, Mod) == SettingsError::NotReady, "unready registration fails");
        settings.Load(schemas, values);
        for (const auto& error : settings.LoadErrors()) std::cerr << error.message << '\n';
        check(settings.LoadErrors().empty(), "consumer schema and boundary fixtures load");
        settings.Start();
        input.Initialize({{Mod, {{"toggle", HotkeyInputState::Target::Callback}, {"menu", HotkeyInputState::Target::Menu}}}});

        check(!api.Read("GetBool", Mod, "enabled", true), "bool reads authored value");
        check(api.Read("GetInt", Mod, "count", std::int32_t(-1)) == 3, "int reads authored value");
        check(api.Read("GetFloat", Mod, "volume", -1.0f) == 0.5f, "float reads authored value");
        check(api.Read("GetEnum", Mod, "mode", EnumValue{"fallback"}).value == "normal", "enum returns option ID");
        check(api.Read("GetString", Mod, "caption", std::string{}) == "Hello", "string reads authored bytes");
        check(api.Read("GetInt", Mod, "wideInt", std::int32_t(77)) == 77, "wide integer uses fallback without wrapping");
        check(api.Read("GetFloat", Mod, "wideFloat", 77.0f) == 77.0f, "float overflow uses fallback");
        for (const std::int64_t value : {std::int64_t(INT32_MIN), std::int64_t(INT32_MAX)}) {
            check(Convert<std::int32_t>(SettingValue(value)).value() == value, "inclusive Papyrus int bounds");
        }
        for (const std::int64_t value : {std::int64_t(INT32_MIN) - 1, std::int64_t(INT32_MAX) + 1, INT64_MIN, INT64_MAX}) {
            check(!Convert<std::int32_t>(SettingValue(value)), "integer overflow rejected");
        }
        for (const double value : {double(std::numeric_limits<float>::max()), -double(std::numeric_limits<float>::max()), 0.1, 1e-100}) {
            check(Convert<float>(SettingValue(value)).value() == static_cast<float>(value), "finite float conversion permits ordinary rounding");
        }
        for (const double value : {1e100, -1e100, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            check(!Convert<float>(SettingValue(value)), "overflow and nonfinite float reads rejected");
        }
        check(api.Read("GetString", Mod, "mode", std::string("fallback")) == "fallback", "string getter does not read enums");
        check(api.Read("GetEnum", Mod, "caption", EnumValue{"fallback"}).value == "fallback", "enum getter does not read strings");
        check(api.Read("GetBool", "missing", "enabled", true), "unknown mod uses fallback");
        check(api.Read("GetInt", Mod, "missing", std::int32_t(99)) == 99, "unknown key uses fallback");
        check(!api.Write("SetInt", Mod, "count", std::int64_t(11)), "bounds reject writes");
        check(api.Write("SetEnum", "PAPYRUSEXAMPLE", "MODE", EnumValue{"NORMAL"}), "Papyrus identifiers resolve regardless of pooled casing");
        check(api.Read("GetString", "PAPYRUSEXAMPLE", "CAPTION", std::string{}) == "Hello", "Papyrus getter resolves pooled casing");
        check(!api.Write("SetBool", Mod, "CaseKey", true), "ambiguous case-only setting IDs are rejected");
        check(!settings.GetValue("PAPYRUSEXAMPLE", "caption"), "native mod matching remains exact");
        check(settings.SetValue(Mod, "mode", EnumValue{"NORMAL"}) == SettingsError::InvalidValue, "native enum matching remains exact");
        check(!api.Write("SetString", Mod, "mode", std::string("quiet")), "writes preserve enum/string separation");
        check(!api.Write("SetFloat", Mod, "volume", std::numeric_limits<double>::quiet_NaN()), "nonfinite writes fail");

        check(listeners.Register(instance, Kind::Changes, Mod) == SettingsError::None, "instance change registration succeeds");
        check(listeners.Register(instance, Kind::Changes, "PAPYRUSEXAMPLE") == SettingsError::None, "pooled mod casing deduplicates registration");
        check(listeners.Register(instance, Kind::Changes, "missing") == SettingsError::UnknownMod, "unknown schema cannot be subscribed");
        check(listeners.Register({}, Kind::Changes, Mod) == SettingsError::InvalidArgument, "empty receiver identity rejected");
        check(listeners.Register(global, Kind::Changes, Mod) == SettingsError::None, "Global change registration succeeds");
        settings.DispatchChanges();
        check(events == std::vector<std::string>{"example:change:", "globalexample:change:"}, "one initial full refresh per target");
        events.clear();
        check(listeners.Register(instance, Kind::Changes, Mod) == SettingsError::None, "duplicate succeeds after initial delivery");
        settings.DispatchChanges();
        check(events.empty(), "duplicate registration does not add an initial notification");

        check(api.Write("SetBool", Mod, "enabled", true), "write succeeds after persistence");
        check(TestJson::parse(std::ifstream(values / "papyrusexample.json"))["values"]["enabled"] == true, "successful return already persisted");
        settings.DispatchChanges();
        check(events.size() == 2 && events[0] == "example:change:enabled", "committed write notifies both listeners");
        events.clear();
        check(api.Write("SetBool", Mod, "enabled", true), "equal value is successful");
        settings.DispatchChanges();
        check(events.empty(), "equal write does not notify");
        check(api.Write("SetString", Mod, "caption", std::string("Mixed Case \xC3\xA9")), "UTF-8 text write succeeds");
        check(api.Read("GetString", Mod, "caption", std::string{}) == "Mixed Case \xC3\xA9", "value path preserves case and Unicode");
        check(!api.Write("SetString", Mod, "caption", std::string(129, 'x')), "byte limit is shared with the service");
        settings.DispatchChanges(); events.clear();

        std::filesystem::rename(values, root / "saved-values");
        { std::ofstream file(values); file << "blocks the values directory"; }
        check(!api.Write("SetBool", Mod, "enabled", false), "failed save returns false");
        check(api.Read("GetBool", Mod, "enabled", false), "failed save preserves old value");
        check(!api.ResetMod(Mod), "failed reset save returns false");
        settings.DispatchChanges();
        check(events.empty(), "failed persistence produces no notification");
        std::filesystem::remove(values);
        std::filesystem::rename(root / "saved-values", values);
        check(api.Reset(Mod, "caption"), "single reset succeeds");
        check(api.ResetMod(Mod), "mod reset succeeds");
        settings.DispatchChanges();
        check(events == std::vector<std::string>{"example:change:", "globalexample:change:"}, "service full refresh supersedes individual keys");
        events.clear();

        check(listeners.Register(instance, Kind::Hotkey, Mod, "toggle") == SettingsError::None, "hotkey registration succeeds");
        check(listeners.Register(instance, Kind::Hotkey, "PAPYRUSEXAMPLE", "TOGGLE") == SettingsError::None, "pooled hotkey casing deduplicates registration");
        check(listeners.Register(global, Kind::Hotkey, Mod, "missing") == SettingsError::UnknownHotkey, "unknown hotkey fails");
        check(listeners.Register(global, Kind::Hotkey, Mod, "menu") == SettingsError::TypeMismatch, "menu hotkeys reject Papyrus handlers");
        const auto press = [&] { return input.ProcessButton(0, 0x75, toggle, 1, 0); };
        check(press() && events == std::vector<std::string>{"example:hotkey:toggle"}, "hotkey submits at admission without a bridge task or poll");
        check(!input.ProcessButton(0, 0x75, toggle, 1, 1) && !input.ProcessButton(0, 0x75, toggle, 0, 1), "repeats and releases do not activate");
        check(events == std::vector<std::string>{"example:hotkey:toggle"}, "one callback per fresh down");
        events.clear();
        const auto block = input.AcquireBlock();
        check(!press(), "native block suppresses Papyrus input too");
        input.ReleaseBlock(block);
        press();
        check(events == std::vector<std::string>{"example:hotkey:toggle"}, "registration remains active after a native block ends");
        events.clear();

        onSubmit = [&] {
            listeners.Suspend();
            listeners.Resume();
        };
        press(); press();
        onSubmit = {};
        check(events.size() == 2, "each fresh press submits directly across a cancelled transition");
        events.clear();

        listeners.Suspend(2);
        listeners.Suspend(4);
        listeners.Resume(4);
        check(listeners.IsSuspended(), "refusing a second load cannot resume delivery during the first load");
        listeners.Resume(2);
        check(!listeners.IsSuspended(), "cancelling the final load resumes the surviving session");

        listeners.Suspend();
        check(listeners.Register(global, Kind::Hotkey, Mod, "toggle") == SettingsError::NotReady, "registration is refused during transition");
        api.Write("SetBool", Mod, "enabled", true);
        settings.DispatchChanges(); press();
        check(events.empty(), "transition suspends delivery");
        listeners.Resume();
        check(events == std::vector<std::string>{"example:change:", "globalexample:change:"}, "cancelled load immediately refreshes changed listeners without replaying presses");
        events.clear();
        listeners.Resume();
        check(events.empty(), "refresh flags are consumed once");
        press();
        check(events == std::vector<std::string>{"example:hotkey:toggle"}, "fresh input resumes after cancellation");
        events.clear();

        listeners.Suspend();
        api.Write("SetBool", Mod, "enabled", false); settings.DispatchChanges(); press();
        listeners.Clear(); listeners.Resume();
        check(events.empty(), "replacement discards pending refreshes");
        check(!press(), "replacement removes internal hotkey registrations");
        check(api.Write("SetBool", Mod, "enabled", true), "values still work after session cleanup");
        settings.DispatchChanges();
        check(events.empty(), "replacement removes instance and Global change registrations");
        check(listeners.Register(instance, Kind::Changes, Mod) == SettingsError::None, "receiver can register again in the new session");
        onSubmit = [&] { listeners.Clear(); };
        settings.DispatchChanges();
        onSubmit = {};
        check(listeners.IsSuspended() && events == std::vector<std::string>{"example:change:"}, "session cleanup during submission preserves the submitted call");
        events.clear();
        listeners.Resume();
        api.Write("SetBool", Mod, "enabled", false); settings.DispatchChanges();
        check(events.empty(), "cleanup during submission detaches the source listener");

        check(listeners.Register(instance, Kind::Hotkey, Mod, "toggle") == SettingsError::None, "hotkey can register again in the new session");
        std::promise<void> entered, finish, clearStarted;
        auto done = finish.get_future();
        onSubmit = [&] { entered.set_value(); done.wait(); };
        auto submit = std::async(std::launch::async, press);
        entered.get_future().wait();
        auto clear = std::async(std::launch::async, [&] { clearStarted.set_value(); listeners.Clear(); });
        clearStarted.get_future().wait();
        const bool waited = clear.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout;
        finish.set_value();
        check(submit.get(), "concurrent input was admitted");
        clear.get();
        check(waited, "session cleanup waits for an active submission");
        onSubmit = {};
        check(!press(), "completed session cleanup leaves no hotkey observer");
        events.clear();

        HotkeyInputState mixedInput;
        mixedInput.Initialize({{Mod, {{"toggle", HotkeyInputState::Target::Callback}}}});
        Subscriptions mixed(settings, mixedInput, collect);
        int nativeCalls{};
        mixedInput.Register(Mod, "toggle", +[](const char*, const char*, void* user) noexcept {
            ++*static_cast<int*>(user);
        }, &nativeCalls);
        check(mixed.Register(global, Kind::Hotkey, Mod, "toggle") == SettingsError::None, "Global hotkey target registers alongside native callback");
        mixedInput.ProcessButton(0, 0x75, toggle, 1, 0);
        check(events == std::vector<std::string>{"globalexample:hotkey:toggle"} && nativeCalls == 1, "Global and native callbacks both run inline");
        events.clear();
        mixedInput.ProcessButton(0, 0x75, toggle, 1, 0);
        mixed.Clear(); mixed.Resume();
        check(events == std::vector<std::string>{"globalexample:hotkey:toggle"} && nativeCalls == 2, "session cleanup preserves already delivered Papyrus and native calls");

        DiagnosticsService issueService;
        API::DiagnosticsApi issueAdapter(issueService);
        Issues issues(issueAdapter);
        check(issues.ReportIssue("scriptmod", "missing-pack", "Missing animations", false, "Uses defaults", "Install the pack"),
            "Papyrus can report a warning without a settings schema");
        auto reported = issueService.Snapshot();
        check(reported.size() == 1 && reported[0].severity == IssueSeverity::Warning &&
            reported[0].impact == "Uses defaults" && reported[0].nextSteps == "Install the pack",
            "Papyrus details reach the shared diagnostics service");
        check(issues.ReportIssue("scriptmod", "missing-pack", "Animation failed", true, "", ""),
            "Papyrus can replace a warning with an error");
        reported = issueService.Snapshot();
        check(reported.size() == 1 && reported[0].severity == IssueSeverity::Error &&
            reported[0].title == "Animation failed" && reported[0].impact.empty() && reported[0].nextSteps.empty(),
            "replacement clears omitted details and does not duplicate the issue");
        check(!issues.ReportIssue("Bad/Mod", "missing-pack", "Invalid", false, "", "") &&
            !issues.ReportIssue("scriptmod", "missing-pack", " ", false, "", "") &&
            issueService.Snapshot().size() == 1, "invalid Papyrus reports leave current issues unchanged");
        check(!issues.ClearIssue("scriptmod", " ") && !issues.ClearModIssues("Bad/Mod") &&
            issueService.Snapshot().size() == 1, "invalid Papyrus clear requests fail without changing reports");
        check(issues.ClearIssue("scriptmod", "missing-pack") && issues.ClearIssue("scriptmod", "missing-pack"),
            "Papyrus clear is idempotent");
        check(issues.ReportIssue("scriptmod", "one", "One", false, "", "") &&
            issues.ReportIssue("scriptmod", "two", "Two", true, "", "") &&
            issues.ReportIssue("othermod", "one", "Other", false, "", ""), "Papyrus reports retain per-mod identities");
        check(issues.ClearModIssues("scriptmod") && issues.ClearModIssues("scriptmod") &&
            issueService.Snapshot().size() == 1 && issueService.Snapshot()[0].modId == "othermod",
            "Papyrus clear-mod removes only its own reports and tolerates repetition");

        SettingsService reload;
        reload.Load(schemas, values); reload.Start();
        check(!Values(reload).Read("GetBool", Mod, "enabled", true), "stored settings survive a fresh service instance");
        std::cout << checks << '/' << checks << " Papyrus bridge checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
