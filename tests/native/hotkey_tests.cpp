#include "API/SettingsApi.h"
#include "Input/HotkeyService.h"

#include <chrono>
#include <fstream>
#include <future>
#include <stdexcept>

namespace
{
    using namespace OSFSettings;
    using API::Status;
    using namespace std::chrono_literals;

    struct Events
    {
        std::vector<std::string> keys;
        std::thread::id thread;
        static void Fired(const char* mod, const char* key, void* context) noexcept
        {
            auto& self = *static_cast<Events*>(context);
            self.keys.emplace_back(std::string(mod) + "/" + key);
            self.thread = std::this_thread::get_id();
        }
    };
}

int TestHotkeys()
{
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        ++checks;
        if (!passed) throw std::runtime_error(std::string("Hotkeys: ") + message);
    };
    const auto root = std::filesystem::temp_directory_path() /
        ("osfsettings-hotkeys-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup
    {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{ root };
    std::filesystem::create_directories(root / "schemas");
    {
        std::ofstream file(root / "schemas" / "sample.json");
        file << R"({"schemaVersion":1,"id":"sample","groups":{"general":[
            {"key":"first","type":"key","default":115,"allowUnbound":true},
            {"key":"second","type":"key","default":115},
            {"key":"enabled","type":"bool","default":true}
        ]}})";
    }
    SettingsService settings;
    HotkeyService hotkeys{ settings };
    API::SettingsApi api{ settings, hotkeys };
    API::Client client;
    API::Subscription token = 99;
    API::Suppression suppression = 99;
    Events events;
    check(client.SubscribeHotkey("sample", "first", Events::Fired, &events, &token) == Status::NotReady && token == 99 &&
        client.AcquireHotkeySuppression(&suppression) == Status::NotReady && suppression == 99 &&
        client.UnsubscribeHotkey(1) == Status::NotReady && client.ReleaseHotkeySuppression(1) == Status::NotReady,
        "detached clients preserve outputs");
    check(client.Attach(&api), "client attaches");
    check(client.SubscribeHotkey("sample", "first", Events::Fired, &events, &token) == Status::NotReady && token == 99,
        "hotkey subscription waits for loaded settings");
    check(client.SubscribeHotkey(nullptr, "first", Events::Fired, &events, &token) == Status::InvalidArgument &&
        client.SubscribeHotkey("sample", nullptr, Events::Fired, &events, &token) == Status::InvalidArgument &&
        client.SubscribeHotkey("sample", "first", nullptr, &events, &token) == Status::InvalidArgument &&
        client.SubscribeHotkey("sample", "first", Events::Fired, &events, nullptr) == Status::InvalidArgument &&
        client.AcquireHotkeySuppression(nullptr) == Status::InvalidArgument, "null argument validation");
    check(client.AcquireHotkeySuppression(&suppression) == Status::Ok && suppression != 0 &&
        client.ReleaseHotkeySuppression(suppression) == Status::Ok, "suppression may be acquired before settings readiness");

    settings.Load(root / "schemas", root / "values");
    settings.Start();
    check(settings.LoadErrors().empty(), "fixture loaded");
    check(client.SubscribeHotkey("BAD", "first", Events::Fired, &events, &token) == Status::InvalidArgument &&
        client.SubscribeHotkey("sample", "", Events::Fired, &events, &token) == Status::InvalidArgument &&
        client.SubscribeHotkey("missing", "first", Events::Fired, &events, &token) == Status::UnknownMod &&
        client.SubscribeHotkey("sample", "missing", Events::Fired, &events, &token) == Status::UnknownSetting &&
        client.SubscribeHotkey("sample", "enabled", Events::Fired, &events, &token) == Status::TypeMismatch && token == 99,
        "subscriptions require an existing key setting and preserve outputs on failure");

    std::string mod = "sample", key = "first";
    check(client.SubscribeHotkey(mod.c_str(), key.c_str(), Events::Fired, &events, &token) == Status::Ok && token,
        "subscribe to key");
    mod = "changed"; key = "changed";
    hotkeys.Dispatch();
    check(events.keys.empty(), "no initial hotkey replay");
    const auto press = [&](std::uint32_t code) { hotkeys.OnButton(code, true, false); };
    const auto release = [&](std::uint32_t code) { hotkeys.OnButton(code, false, false); };
    const auto tap = [&](std::uint32_t code) { press(code); release(code); hotkeys.Dispatch(); };

    tap(115);
    check(events.keys.empty(), "gameplay starts blocked");
    hotkeys.SetInputAllowed(true);
    press(115);
    hotkeys.OnButton(115, true, true);
    press(115);
    hotkeys.Dispatch();
    check(events.keys == std::vector<std::string>{ "sample/first" } && events.thread == std::this_thread::get_id(),
        "one callback per press on the dispatch thread, with owned mod and key names");
    release(115);
    hotkeys.Dispatch();
    check(events.keys.size() == 1, "release does not fire");
    tap(116); tap(0); tap(255); tap(999);
    check(events.keys.size() == 1, "unbound, invalid and unrelated keys do not fire");

    check(client.SetKey("sample", "first", 116) == Status::Ok, "rebind saves");
    tap(115); tap(116);
    check(events.keys.size() == 2, "rebind applies immediately without a settings-notification pump");
    press(116);
    check(client.SetKey("sample", "first", 117) == Status::Ok, "change after press");
    hotkeys.Dispatch(); release(116);
    check(events.keys.size() == 2, "stale queued binding is discarded");
    check(client.SetKey("sample", "first", 255) == Status::Ok, "unbinding saves");
    tap(117);
    check(events.keys.size() == 2, "unbound setting does not fire");
    check(client.Reset("sample", "first") == Status::Ok, "reset key");
    tap(115);
    check(events.keys.size() == 3, "reset takes effect immediately");

    // A failed write must not change which physical key invokes the consumer.
    const auto saved = root / "values" / "sample.json";
    std::filesystem::create_directory(saved.string() + ".tmp");
    check(client.SetKey("sample", "first", 116) == Status::SaveFailed, "failed rebind is reported");
    tap(116); tap(115);
    check(events.keys.size() == 4, "failed rebind preserves the working key");
    std::filesystem::remove(saved.string() + ".tmp");

    events.keys.clear();
    press(115);
    hotkeys.SetInputAllowed(false);
    hotkeys.SetInputAllowed(true);
    hotkeys.Dispatch(); press(115); hotkeys.Dispatch(); release(115);
    check(events.keys.empty(), "menu transition drops pending presses and requires held-key release");
    hotkeys.SetInputAllowed(false); press(115);
    hotkeys.SetInputAllowed(true); hotkeys.OnButton(115, true, true); hotkeys.Dispatch(); release(115);
    check(events.keys.empty(), "a press made in a menu never replays into gameplay");
    tap(115);
    check(events.keys.size() == 1, "fresh press after menu fires");

    API::Suppression first{}, second{};
    check(client.AcquireHotkeySuppression(&first) == Status::Ok &&
        client.AcquireHotkeySuppression(&second) == Status::Ok && first != second,
        "independent suppression owners receive distinct tokens");
    tap(115);
    check(client.ReleaseHotkeySuppression(first) == Status::Ok, "release one suppression");
    tap(115);
    check(events.keys.size() == 1, "another owner's suppression remains active");
    press(115);
    check(client.ReleaseHotkeySuppression(second) == Status::Ok, "release final suppression");
    press(115); hotkeys.Dispatch(); release(115);
    check(events.keys.size() == 1, "suppressed held key waits for release");
    press(115);
    client.AcquireHotkeySuppression(&first); client.ReleaseHotkeySuppression(first);
    hotkeys.Dispatch(); release(115);
    check(events.keys.size() == 1, "even a completed suppression interval invalidates pending presses");
    tap(115);
    check(events.keys.size() == 2, "fresh press works after suppression ends");
    check(client.ReleaseHotkeySuppression(second) == Status::UnknownSuppression &&
        client.ReleaseHotkeySuppression(0) == Status::UnknownSuppression, "stale suppression tokens cannot release another owner");

    API::Subscription another{};
    press(115);
    client.SubscribeHotkey("sample", "second", Events::Fired, &events, &another);
    hotkeys.Dispatch(); release(115);
    check(events.keys.back() == "sample/first", "new subscribers do not receive earlier presses");
    events.keys.clear(); tap(115);
    check(events.keys == std::vector<std::string>{ "sample/first", "sample/second" }, "all registered bindings on a shared key fire");
    press(115);
    check(client.UnsubscribeHotkey(token) == Status::Ok, "unsubscribe with a queued press");
    events.keys.clear(); hotkeys.Dispatch(); release(115);
    check(events.keys == std::vector<std::string>{ "sample/second" }, "unsubscribe cancels queued delivery");
    client.UnsubscribeHotkey(another);
    check(client.UnsubscribeHotkey(token) == Status::UnknownSubscription &&
        client.UnsubscribeHotkey(0) == Status::UnknownSubscription, "unknown hotkey subscription status");

    HotkeyService::Subscription self{};
    bool selfRemoved{};
    hotkeys.Subscribe("sample", "first", [&](const auto&, const auto&) {
        selfRemoved = hotkeys.Unsubscribe(self) == SettingsError::None;
        hotkeys.Dispatch(); // Recursive pumping must not deliver concurrently.
    }, self);
    tap(115); tap(115);
    check(selfRemoved, "callback can unsubscribe itself without deadlocking");

    HotkeyService::Subscription blocker{}, blocked{};
    bool firstCallback{}, blockedCallback{};
    hotkeys.Subscribe("sample", "first", [&](const auto&, const auto&) {
        firstCallback = true;
        HotkeyService::Suppression lease{};
        hotkeys.AcquireSuppression(lease); hotkeys.ReleaseSuppression(lease);
    }, blocker);
    hotkeys.Subscribe("sample", "second", [&](const auto&, const auto&) { blockedCallback = true; }, blocked);
    tap(115);
    check(firstCallback && !blockedCallback, "suppression from a callback cancels the remaining batch");
    hotkeys.Unsubscribe(blocker); hotkeys.Unsubscribe(blocked);

    // Unsubscribe must not return while a different thread still uses the callback's captures.
    std::promise<void> entered, finish, unsubscribing;
    auto enteredFuture = entered.get_future();
    auto finishFuture = finish.get_future().share();
    auto unsubscribingFuture = unsubscribing.get_future();
    hotkeys.Subscribe("sample", "first", [&](const auto&, const auto&) {
        entered.set_value();
        finishFuture.wait();
    }, self);
    press(115);
    auto dispatch = std::async(std::launch::async, [&] { hotkeys.Dispatch(); });
    const bool invoked = enteredFuture.wait_for(2s) == std::future_status::ready;
    auto removal = std::async(std::launch::async, [&] {
        unsubscribing.set_value();
        return hotkeys.Unsubscribe(self);
    });
    unsubscribingFuture.wait();
    const bool waiting = removal.wait_for(30ms) == std::future_status::timeout;
    finish.set_value();
    dispatch.get();
    const auto removed = removal.get();
    release(115);
    check(invoked && waiting && removed == SettingsError::None, "unsubscribe waits for an in-flight callback on another thread");

    // Captures are released outside the hotkey mutex, including cancelled pending events.
    bool destroyed{};
    auto owner = std::shared_ptr<int>(new int, [&](int* value) {
        HotkeyService::Suppression lease{};
        hotkeys.AcquireSuppression(lease); hotkeys.ReleaseSuppression(lease);
        destroyed = true; delete value;
    });
    hotkeys.Subscribe("sample", "first", [owner](const auto&, const auto&) {}, self);
    owner.reset(); press(115);
    hotkeys.Unsubscribe(self); hotkeys.SetInputAllowed(false);
    check(destroyed, "cancelled listeners release captures without holding the service lock");
    return checks;
}
