#include "SFSE/Impl/PCH.h"
#include "API/SettingsApi.h"
#include "Input/HotkeyService.h"
#include "RE/B/BSInputEventUser.h"
#include "Settings/SettingsJson.h"
#include <array>
#include <chrono>
#include <deque>
#include <fstream>
#include <future>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace
{
    using namespace OSFSettings;
    using API::Status;
    using namespace std::chrono_literals;
    struct Events
    {
        std::vector<std::string> actions;
        std::thread::id thread;
        static void Fired(const char* mod, const char* action, void* user) noexcept
        {
            auto& self = *static_cast<Events*>(user);
            self.actions.emplace_back(std::string(mod) + "/" + action);
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
    auto schema = nlohmann::json::parse(R"({"schemaVersion":1,"id":"sample","title":"Sample",
        "groups":[{"id":"general","settings":[{"key":"legacy","type":"key","default":115},
        {"key":"enabled","type":"bool","default":true}]}],
        "hotkeys":[{"id":"first","label":"First"},{"id":"second","label":"Second"}]})");
    std::string error;
    auto parsed = SettingsJson::ParseSchema(schema, error);
    check(parsed && parsed->hotkeys.size() == 2 && parsed->hotkeys[0].id == "first", "declarations parse separately from settings");
    for (const auto id : { "", "bad.id", "bad\tID", "bad\nID", "bad ID" }) {
        auto bad = schema; bad["hotkeys"][0]["id"] = id;
        check(!SettingsJson::ParseSchema(bad, error), "unsafe or ambiguous action ID rejected");
    }
    check(parsed->hotkeys[0].defaultKey == 255, "omitted hotkey default is unbound");
    for (const auto& value : { nlohmann::json("F10"), nlohmann::json("f10"), nlohmann::json(121) }) {
        auto withDefault = schema; withDefault["hotkeys"][0]["default"] = value;
        const auto result = SettingsJson::ParseSchema(withDefault, error);
        check(result && result->hotkeys[0].defaultKey == 121, "schema defaults use the native virtual-key identity");
    }
    for (const auto& value : { nlohmann::json("F01"), nlohmann::json("CTRL+F10"), nlohmann::json("ESCAPE"),
        nlohmann::json(-1), nlohmann::json(256), nlohmann::json(1), nlohmann::json(true), nlohmann::json(12.5), nlohmann::json(nullptr) }) {
        auto withDefault = schema; withDefault["hotkeys"][0]["default"] = value;
        check(!SettingsJson::ParseSchema(withDefault, error), "invalid default cannot reach native parser");
    }
    auto bad = schema; bad["hotkeys"][1]["id"] = "FIRST";
    check(!SettingsJson::ParseSchema(bad, error), "case-only duplicate action rejected");
    for (const auto& [field, value] : std::array<std::pair<const char*, const char*>, 3>{
        {{"context","global"},{"trigger","hold"},{"default","F25"}}}) {
        bad = schema; bad["hotkeys"][0][field] = value;
        check(!SettingsJson::ParseSchema(bad, error), "unsupported action semantics rejected rather than ignored");
    }
    bad = schema; bad["hotkeys"][0]["label"] = std::string("x\0y",3);
    check(!SettingsJson::ParseSchema(bad, error), "embedded NUL label rejected");
    check(NativeHotkeyName("one.two", "three") != NativeHotkeyName("one", "two_three"), "namespaced action identity is unambiguous");
    const auto root = std::filesystem::temp_directory_path() /
        ("osf-native-hotkeys-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); } } cleanup{root};
    std::filesystem::create_directories(root / "schemas");
    { std::ofstream file(root / "schemas/sample.json"); file << schema; }
    SettingsService settings;
    HotkeyService hotkeys{settings};
    API::SettingsApi api{settings,hotkeys};
    API::Client client;
    API::Subscription token=99, second=99;
    API::HotkeyBlock block=99;
    Events firstEvents, secondEvents;
    check(client.SubscribeHotkey("sample","first",Events::Fired,&firstEvents,&token)==Status::NotReady && token==99,
        "detached client preserves subscription output");
    check(client.Attach(&api), "client attaches");
    check(client.SubscribeHotkey("sample","first",Events::Fired,&firstEvents,&token)==Status::NotReady && token==99,
        "subscription fails before backend readiness");
    check(client.AcquireHotkeyBlock(&block)==Status::Ok && client.ReleaseHotkeyBlock(block)==Status::Ok,
        "focus blocker works before backend readiness");
    settings.Load(root / "schemas", root / "values"); settings.Start();
    check(hotkeys.Configure() && !hotkeys.Configure(), "catalog configured exactly once");
    const auto catalog = hotkeys.Actions();
    check(catalog.size() == 2 && catalog[0].eventName == "OSFSettings.sample.first",
        "published catalog exposes the configured native action identities");
    hotkeys.SetAvailable(true);
    check(client.SubscribeHotkey("sample","legacy",Events::Fired,&firstEvents,&token)==Status::UnknownAction,
        "ordinary key settings are not action declarations");
    check(client.SubscribeHotkey("missing","first",Events::Fired,&firstEvents,&token)==Status::UnknownMod &&
        client.SubscribeHotkey("sample","bad.id",Events::Fired,&firstEvents,&token)==Status::InvalidArgument && token==99,
        "invalid subscription preserves output");
    check(client.SubscribeHotkey(nullptr,"first",Events::Fired,&firstEvents,&token)==Status::InvalidArgument &&
        client.SubscribeHotkey("sample","first",nullptr,nullptr,&token)==Status::InvalidArgument, "API null argument validation");
    check(client.SubscribeHotkey("sample","first",Events::Fired,&firstEvents,&token)==Status::Ok &&
        client.SubscribeHotkey("sample","second",Events::Fired,&secondEvents,&second)==Status::Ok,
        "subscriptions exist independently of unbound native assignments");
    const auto dispatch=[&] { hotkeys.Dispatch([] {return true;}); };
    dispatch(); check(firstEvents.actions.empty(), "no initial activation replay");
    const auto press=[&](std::string_view action, std::uint32_t key=115, bool repeat=false) {
        RE::ButtonEvent event;
        event.eventType = RE::InputEvent::EventType::kButton;
        event.deviceType = RE::InputEvent::DeviceType::kKeyboard;
        event.idCode = key;
        event.value = 1;
        event.heldDownSecs = repeat ? 0.2f : 0.0f;
        event.strUserEvent = std::string(action).c_str();
        hotkeys.ProcessInput(&event, [] {});
    };
    const auto release=[&](std::uint32_t key=115) {
        RE::ButtonEvent event;
        event.eventType = RE::InputEvent::EventType::kButton;
        event.deviceType = RE::InputEvent::DeviceType::kKeyboard;
        event.idCode = key;
        hotkeys.ProcessInput(&event, [] {});
    };
    const auto tap=[&](std::string_view action, std::uint32_t key=115) { press(action,key); release(key); dispatch(); };
    constexpr std::string_view first="OSFSettings.sample.first", other="OSFSettings.sample.second";
    press(first); release(); hotkeys.Dispatch([] { return false; });
    check(firstEvents.actions.empty(), "ineligible gameplay drops the first queued press");
    press(first); press(first,115,true); release(); dispatch();
    check(firstEvents.actions.size()==1 && secondEvents.actions.empty(),
        "one activation per press; native action identity prevents shared-key fanout");
    tap(other); check(secondEvents.actions.size()==1 && firstEvents.actions.size()==1, "second native name selects only its own subscribers");
    tap(first,116); check(firstEvents.actions.size()==2, "resolved action delivery does not depend on its physical key");
    check(firstEvents.thread==std::this_thread::get_id(), "callbacks execute on the supplied dispatch phase, not observation");
    const auto beforeRemap = hotkeys.CurrentGeneration();
    press(first); release(); hotkeys.Invalidate(); hotkeys.Invalidate(); dispatch();
    check(firstEvents.actions.size()==2, "native remap notifications cancel queued activations; duplicate notifications are harmless");
    hotkeys.Activate(first,beforeRemap); dispatch();
    check(firstEvents.actions.size()==2, "an observation captured before remap cannot be queued afterward");
    tap(first,117); check(firstEvents.actions.size()==3, "fresh named action works after invalidation without resubscribing or refreshing bindings");
    hotkeys.Invalidate(); dispatch();
    press(""); release(); dispatch();
    check(firstEvents.actions.size()==3, "physical input without a resolved native action cannot activate a subscriber");
    tap("OSFSettings.sample.missing");
    check(firstEvents.actions.size()==3 && secondEvents.actions.size()==1, "unknown native action names cannot activate subscribers");
    check(!std::filesystem::exists(root / "values/sample.json"), "native action delivery never writes OSF binding values");
    press(first); hotkeys.Dispatch([] { return false; }); press(first,115,true); dispatch();
    check(firstEvents.actions.size()==3, "blocked held press does not reactivate on resume");
    release(); tap(first); check(firstEvents.actions.size()==4, "release and fresh press recover after blocking");
    press(first); release(); client.AcquireHotkeyBlock(&block); client.ReleaseHotkeyBlock(block); dispatch();
    check(firstEvents.actions.size()==4, "block acquire/release invalidates the pending batch");
    API::HotkeyBlock block2{}; client.AcquireHotkeyBlock(&block); client.AcquireHotkeyBlock(&block2);
    client.ReleaseHotkeyBlock(block); tap(first); check(firstEvents.actions.size()==4, "nested blocker retains suppression");
    client.ReleaseHotkeyBlock(block2);
    check(client.ReleaseHotkeyBlock(block2)==Status::UnknownHotkeyBlock, "stale blocker token rejected");
    press(first); release(); hotkeys.Dispatch([]{return true;},HotkeyService::Clock::now()+1s);
    check(firstEvents.actions.size()==4, "delayed activation expires instead of replaying after a stall");
    press(first); release(); hotkeys.Dispatch([]{return false;}); dispatch();
    check(firstEvents.actions.size()==4, "eligibility rechecked at delivery and discarded permanently");
    press(first); release(); hotkeys.Invalidate(); dispatch();
    check(firstEvents.actions.size()==4, "reset/reload epoch invalidates pre-transition events");
    press(first); release(); client.UnsubscribeHotkey(token); dispatch();
    check(firstEvents.actions.size()==4 && client.UnsubscribeHotkey(token)==Status::UnknownSubscription, "unsubscribe cancels queued calls");
    HotkeyService::Subscription self{};
    int selfCalls{};
    hotkeys.Subscribe("sample","first",[&](const auto&,const auto&){++selfCalls; hotkeys.Unsubscribe(self);},self);
    tap(first); tap(first); check(selfCalls==1, "self unsubscribe prevents later calls without deadlocking");
    std::promise<void> entered,finish,unsubscribing;
    auto enteredFuture=entered.get_future(); auto finishFuture=finish.get_future().share(); auto unsubFuture=unsubscribing.get_future();
    hotkeys.Subscribe("sample","first",[&](const auto&,const auto&){entered.set_value(); finishFuture.wait();},self);
    press(first); release();
    auto running=std::async(std::launch::async,[&]{dispatch();});
    const bool invoked=enteredFuture.wait_for(2s)==std::future_status::ready;
    auto removing=std::async(std::launch::async,[&]{unsubscribing.set_value(); return hotkeys.Unsubscribe(self);});
    unsubFuture.wait(); const bool waited=removing.wait_for(30ms)==std::future_status::timeout;
    finish.set_value(); running.get(); const auto removed=removing.get();
    check(invoked && waited && removed==SettingsError::None, "unsubscribe waits for callback lifetime on another thread");
    bool destroyed{};
    auto owner=std::shared_ptr<int>(new int,[&](int* value){HotkeyService::HotkeyBlock lease{};hotkeys.AcquireBlock(lease);hotkeys.ReleaseBlock(lease);destroyed=true;delete value;});
    hotkeys.Subscribe("sample","first",[owner](const auto&,const auto&){},self); owner.reset();
    press(first); release(); hotkeys.Unsubscribe(self); hotkeys.Invalidate();
    check(destroyed, "callback captures destroyed outside service lock");
    HotkeyService::Subscription invalidator{}, cancelled{};
    int firstInBatch{}, secondInBatch{};
    hotkeys.Subscribe("sample","first",[&](const auto&,const auto&){++firstInBatch;hotkeys.Invalidate();},invalidator);
    hotkeys.Subscribe("sample","first",[&](const auto&,const auto&){++secondInBatch;},cancelled);
    tap(first);
    check(firstInBatch==1 && secondInBatch==0, "invalidation during delivery also cancels the rest of the detached batch");
    hotkeys.Unsubscribe(invalidator); hotkeys.Unsubscribe(cancelled);
    press(other); release(); hotkeys.SetAvailable(false); dispatch();
    check(secondEvents.actions.size()==1 &&
        client.SubscribeHotkey("sample","first",Events::Fired,&firstEvents,&token)==Status::NotReady,
        "backend failure cancels delivery and never falls back to raw-key matching");
    check(hotkeys.Actions().data() == catalog.data() && catalog[0].eventName == "OSFSettings.sample.first",
        "catalog views survive subscriptions, callbacks, remaps, blocks and backend failure");

    HotkeyService input{settings};
    check(input.Configure(), "input-batch service configured");
    input.SetAvailable(true);
    int activations{}, secondActivations{}, originalCalls{};
    HotkeyService::Subscription inputToken{};
    input.Subscribe("sample", "first", [&](const auto&, const auto&) { ++activations; }, inputToken);
    input.Subscribe("sample", "second", [&](const auto&, const auto&) { ++secondActivations; }, inputToken);
    RE::ButtonEvent button;
    button.eventType = RE::InputEvent::EventType::kButton;
    button.deviceType = RE::InputEvent::DeviceType::kKeyboard;
    button.idCode = 121;
    button.strUserEvent = "OSFSettings.sample.first";
    const auto deliver = [&](const std::function<void()>& native = [] {}) {
        input.ProcessInput(&button, [&] { ++originalCalls; native(); });
        input.Dispatch([] { return true; });
    };
    const auto releaseInput = [&] {
        button.value = 0;
        button.heldDownSecs = 0;
        button.disabled = 0;
        button.status = RE::InputEvent::Status::kUnhandled;
        button.strUserEvent = "OSFSettings.sample.first";
        deliver();
        button.value = 1;
        ++button.timeCode;
    };
    releaseInput();
    const auto beforeOriginal = originalCalls;
    deliver([&] { check(activations == 0, "native receiver runs before plugin activation"); });
    check(activations == 1 && originalCalls == beforeOriginal + 1, "one fresh native press calls the original once and activates once");
    button.heldDownSecs = 0.2f;
    deliver();
    check(activations == 1, "vanilla held events do not reactivate");
    releaseInput(); deliver([&] { button.status = RE::InputEvent::Status::kStop; });
    check(activations == 1, "native UI consumption cancels the observed press");
    releaseInput(); deliver([&] { button.disabled = 4; });
    check(activations == 1, "native suppression after observation cancels the press");
    releaseInput(); deliver([&] { button.strUserEvent = "OSFSettings.sample.second"; });
    check(activations == 1 && secondActivations == 1, "vanilla's final resolved action selects the subscriber");
    releaseInput(); deliver([&] { input.Invalidate(); });
    check(activations == 1, "remapping during native processing invalidates the whole batch");
    releaseInput(); deliver([&] { HotkeyService::HotkeyBlock lease{}; input.AcquireBlock(lease); input.ReleaseBlock(lease); });
    check(activations == 1, "a temporary block during native processing also cancels the batch");
    releaseInput(); button.disabled = 4; deliver([&] { button.disabled = 0; });
    check(activations == 2, "vanilla's final enabled event state determines delivery");
    releaseInput(); deliver();
    check(activations == 3, "release and a fresh native press recover normally");
    releaseInput(); button.strUserEvent = "";
    deliver([&] { button.strUserEvent = "OSFSettings.sample.first"; });
    check(activations == 4, "an action resolved by vanilla during processing is delivered");
    releaseInput(); button.deviceType = RE::InputEvent::DeviceType::kMouse; deliver();
    check(activations == 4, "the keyboard-only callback contract does not admit mouse events");
    HotkeyService scheduled{settings};
    std::deque<std::function<void()>> tasks;
    bool eligible = true;
    int scheduledCalls{};
    check(scheduled.Configure([&] {
        (void)scheduled.CurrentGeneration(); // Scheduling must not hold the service lock.
        tasks.emplace_back([&] { scheduled.Dispatch([&] { return eligible; }); });
    }), "event-driven hotkey service configured");
    scheduled.SetAvailable(true);
    const auto queuePress = [&] { scheduled.Activate(first, scheduled.CurrentGeneration()); };
    const auto runTask = [&] {
        auto task = std::move(tasks.front());
        tasks.pop_front();
        task();
    };
    queuePress();
    check(tasks.empty(), "an action without subscribers does not schedule a task");
    HotkeyService::Subscription scheduledToken{};
    scheduled.Subscribe("sample", "first", [&](const auto&, const auto&) {
        if (++scheduledCalls == 1) queuePress();
    }, scheduledToken);
    queuePress(); queuePress();
    check(tasks.size() == 1 && scheduledCalls == 0, "a burst of activations schedules one task and never calls inline");
    runTask();
    check(tasks.size() == 1 && scheduledCalls == 2, "an activation arriving during delivery schedules a following task");
    runTask();
    check(tasks.empty() && scheduledCalls == 3, "hotkey delivery becomes idle when the pending batch is empty");
    queuePress(); scheduled.Invalidate(); runTask();
    check(tasks.empty() && scheduledCalls == 3, "menu or remap events cancel an already scheduled activation");
    eligible = false;
    queuePress(); runTask();
    eligible = true;
    check(tasks.empty() && scheduledCalls == 3, "ineligible delivery is discarded without a background retry");
    queuePress(); runTask();
    check(tasks.empty() && scheduledCalls == 4, "the first fresh press after gameplay resumes works without a cached eligibility refresh");
    scheduled.AcquireBlock(block);
    queuePress();
    check(tasks.empty(), "blocked input schedules no work");
    scheduled.ReleaseBlock(block);
    queuePress(); scheduled.Unsubscribe(scheduledToken); runTask();
    check(tasks.empty() && scheduledCalls == 4, "unsubscribe cancels an event-driven activation");
    return checks;
}
