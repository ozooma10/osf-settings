#include "API/SettingsApi.h"
#include "Input/HotkeyInputState.h"
#include "Settings/SettingsService.h"
#include "Settings/SettingsJson.h"

#include <iostream>
#include <chrono>
#include <future>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>

extern "C" void* OSFSettings_RequestAPI(std::uint32_t, std::uint32_t*) noexcept;

namespace
{
    using namespace OSFSettings;
    using API::Status;
    using Json = nlohmann::ordered_json;
    using namespace std::chrono_literals;

    struct Events
    {
        API::ISettings* service{};
        std::vector<std::string> keys;
        std::thread::id thread;
        bool valid{ true };

        static void Changed(const char* mod, const char* key, void* context) noexcept
        {
            auto& self = *static_cast<Events*>(context);
            self.thread = std::this_thread::get_id();
            self.valid = self.valid && std::string_view(mod) == "sample";
            bool value{};
            self.valid = self.valid && self.service->GetBool(mod, "enabled", &value) == Status::Ok;
            self.keys.emplace_back(key ? key : "*");
        }
    };

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    }
}

int TestSettingsService()
{
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        ++checks;
        if (!passed) throw std::runtime_error(std::string("Settings service: ") + message);
    };
    const auto root = std::filesystem::temp_directory_path() /
        ("osfsettings-service-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup
    {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{ root };
    const auto schemas = root / "schemas";
    const auto values = root / "values";
    const auto saved = values / "sample.json";
    std::filesystem::create_directories(schemas);
    const auto schema = Json::parse(R"({
        "schemaVersion": 1, "id": "sample", "groups": {"main": [
            {"key": "enabled", "type": "bool", "default": false},
            {"key": "count", "type": "int", "default": 3, "min": 0, "max": 10},
            {"key": "scale", "type": "float", "default": 0.15, "min": 0, "max": 1, "step": 0.1},
            {"key": "mode", "type": "enum", "default": "quiet", "options": ["quiet", "verbose-mode"]}
        ]}
    })");
    { std::ofstream output(schemas / "sample.json"); output << schema.dump(); }

    std::uint32_t version = 42;
    check(!OSFSettings_RequestAPI(0x00020000u, &version) && version == 0, "export rejects a different major");
    version = 42;
    check(!OSFSettings_RequestAPI(API::kVersion + 1, &version) && version == 0, "export rejects a newer minor");
    auto* exported = static_cast<API::ISettings*>(OSFSettings_RequestAPI(API::kBaseVersion, &version));
    check(exported && version == API::kVersion && !exported->IsReady() &&
        OSFSettings_RequestAPI(API::kBaseVersion, nullptr) == exported, "export returns the same interface before readiness");

    SettingsService backend;
    HotkeyInputState hotkeys;
    API::SettingsApi service{ backend, hotkeys };
    Events events{ &service };
    API::Subscription token{};
    std::string watched = "sample";
    check(service.Subscribe(watched.c_str(), &Events::Changed, &events, &token) == Status::Ok && token != 0,
        "subscribe before readiness");
    watched = "changed-caller-storage";
    bool enabled = true;
    check(service.GetBool(nullptr, "enabled", &enabled) == Status::InvalidArgument && enabled, "arguments precede readiness");
    check(service.GetBool("sample", "enabled", &enabled) == Status::NotReady && enabled, "read before readiness preserves output");
    check(service.SetBool("sample", "enabled", false) == Status::NotReady &&
        service.Reset("sample", "enabled") == Status::NotReady && service.ResetMod("sample") == Status::NotReady,
        "writes and resets wait for readiness");
    backend.DispatchChanges();
    check(events.keys.empty() && !backend.HasPendingChanges(), "initial refresh waits for readiness");
    backend.Load(schemas, values);
    check(!service.IsReady() && backend.LoadErrors().empty(), "loading alone does not start the service");
    backend.Start();
    check(service.IsReady() && backend.HasPendingChanges(), "starting enables pending notifications");
    backend.DispatchChanges();
    check(events.keys == std::vector<std::string>{ "*" } && events.valid, "initial refresh owns the subscribed mod string and permits reads");
    backend.DispatchChanges();
    check(events.keys.size() == 1 && !backend.HasPendingChanges(), "initial refresh is delivered once");
    events.keys.clear();

    check(service.GetBool("BAD", "enabled", &enabled) == Status::InvalidArgument &&
        service.GetBool("..", "enabled", &enabled) == Status::InvalidArgument &&
        service.GetBool("sample", "", &enabled) == Status::InvalidArgument &&
        service.GetBool("sample", "enabled", nullptr) == Status::InvalidArgument, "read argument validation");
    check(service.GetBool("missing", "enabled", &enabled) == Status::UnknownMod &&
        service.GetBool("sample", "missing", &enabled) == Status::UnknownSetting &&
        service.GetBool("sample", "count", &enabled) == Status::TypeMismatch && enabled, "read errors preserve scalar output");
    std::int64_t count = -1;
    double scale = -1;
    check(service.GetBool("sample", "enabled", &enabled) == Status::Ok && !enabled &&
        service.GetInt("sample", "count", &count) == Status::Ok && count == 3 &&
        service.GetFloat("sample", "scale", &scale) == Status::Ok && scale == 0.15, "exact typed defaults");
    check(service.SetInt("sample", "enabled", 1) == Status::TypeMismatch &&
        service.SetFloat("sample", "count", 3) == Status::TypeMismatch &&
        service.SetEnum("sample", "enabled", "quiet") == Status::TypeMismatch, "setters never coerce types");
    check(service.SetInt("sample", "count", 11) == Status::InvalidValue &&
        service.SetFloat("sample", "scale", std::numeric_limits<double>::infinity()) == Status::InvalidValue &&
        service.SetFloat("sample", "scale", std::numeric_limits<double>::quiet_NaN()) == Status::InvalidValue &&
        service.SetEnum("sample", "mode", "missing") == Status::InvalidValue, "bounds, finite values, and enum membership");
    check(service.SetBool(nullptr, "enabled", true) == Status::InvalidArgument &&
        service.SetEnum("sample", "mode", nullptr) == Status::InvalidArgument &&
        service.SetBool("missing", "enabled", true) == Status::UnknownMod &&
        service.SetBool("sample", "missing", true) == Status::UnknownSetting, "setter arguments and lookup errors");
    check(service.Reset("sample", "missing") == Status::UnknownSetting &&
        service.ResetMod("missing") == Status::UnknownMod && service.ResetMod("") == Status::InvalidArgument &&
        service.Reset("sample", "") == Status::InvalidArgument, "reset arguments and lookup errors");
    check(!backend.HasPendingChanges(), "rejected writes do not notify");

    API::Client client;
    client.Attach(&service);
    std::string mode = "unchanged";
    check(client.GetEnum("sample", "mode", mode) == Status::Ok && mode == "quiet",
        "SDK string reads use the native enum buffer interface");
    check(client.GetEnum("sample", "enabled", mode) == Status::TypeMismatch &&
        client.GetEnum(nullptr, "mode", mode) == Status::InvalidArgument && mode == "quiet",
        "SDK string reads preserve native errors and leave output unchanged");

    std::uint32_t required = 99;
    check(service.GetEnum("sample", "mode", nullptr, 0, &required) == Status::BufferTooSmall && required == 6, "enum size query includes NUL");
    char text[32] = "unchanged";
    required = 99;
    check(service.GetEnum("sample", "mode", text, 2, &required) == Status::BufferTooSmall &&
        required == 6 && std::string_view(text) == "unchanged", "short enum buffers are not modified");
    required = 99;
    check(service.GetEnum("sample", "mode", nullptr, 1, &required) == Status::InvalidArgument && required == 99 &&
        service.GetEnum("sample", "mode", text, sizeof(text), nullptr) == Status::InvalidArgument &&
        service.GetEnum("sample", "enabled", text, sizeof(text), &required) == Status::TypeMismatch && required == 99,
        "enum errors leave required unchanged");
    check(service.SetEnum("sample", "mode", "verbose-mode") == Status::Ok &&
        service.GetEnum("sample", "mode", text, 6, &required) == Status::BufferTooSmall && required == 13 &&
        service.GetEnum("sample", "mode", text, sizeof(text), &required) == Status::Ok && std::string_view(text) == "verbose-mode",
        "enum callers can retry after growth between query and copy");
    check(mode == "quiet" && client.GetEnum("sample", "mode", mode) == Status::Ok && mode == "verbose-mode",
        "SDK enum strings own their values and can read later changes");
    check(events.keys.empty(), "writes never dispatch inline");
    backend.DispatchChanges();
    check(events.keys == std::vector<std::string>{ "mode" }, "changed keys notify");
    events.keys.clear();
    const auto before = backend.Snapshot();
    check(service.SetBool("sample", "enabled", true) == Status::Ok && service.SetBool("sample", "enabled", false) == Status::Ok &&
        service.SetBool("sample", "enabled", true) == Status::Ok && service.SetInt("sample", "count", 8) == Status::Ok &&
        service.SetFloat("sample", "scale", 0.27) == Status::Ok, "valid writes commit without snapping floats");
    check(std::get<bool>(before[0].values.at("enabled")) == false, "menu snapshots own their values");
    backend.DispatchChanges();
    check(events.keys == std::vector<std::string>({ "count", "enabled", "scale" }) && events.thread == std::this_thread::get_id(),
        "changes coalesce and run on the dispatching thread");
    events.keys.clear();
    const auto persisted = ReadFile(saved);
    check(Json::parse(persisted)["values"]["count"] == 8, "successful writes reach disk before returning");
    std::filesystem::create_directory(values / "sample.json.tmp");
    check(service.SetInt("sample", "count", 9) == Status::SaveFailed && service.ResetMod("sample") == Status::SaveFailed &&
        service.Reset("sample", "enabled") == Status::SaveFailed, "save failures propagate through setters and resets");
    check(service.GetBool("sample", "enabled", &enabled) == Status::Ok && enabled &&
        service.GetInt("sample", "count", &count) == Status::Ok && count == 8 && ReadFile(saved) == persisted &&
        !backend.HasPendingChanges(), "failed atomic reset preserves all live values, disk, and notifications");
    check(service.SetInt("sample", "count", 8) == Status::Ok && !backend.HasPendingChanges(), "unchanged writes skip failed storage");
    std::filesystem::remove(values / "sample.json.tmp");
    check(service.ResetMod("sample") == Status::Ok, "mod reset commits");
    const auto defaults = Json::parse(ReadFile(saved))["values"];
    check(defaults["enabled"] == false && defaults["count"] == 3 && defaults["scale"] == 0.15 && defaults["mode"] == "quiet",
        "mod reset saves all defaults in one transaction");
    backend.DispatchChanges();
    check(events.keys == std::vector<std::string>{ "*" }, "mod reset requests one full refresh");
    events.keys.clear();
    check(service.ResetMod("sample") == Status::Ok && !backend.HasPendingChanges(), "unchanged reset does not notify");
    service.SetFloat("sample", "scale", 0.4);
    check(service.Reset("sample", "scale") == Status::Ok && service.GetFloat("sample", "scale", &scale) == Status::Ok && scale == 0.15,
        "single reset restores the exact off-step default");
    backend.DispatchChanges();
    events.keys.clear();

    Events duplicate{ &service };
    API::Subscription duplicateToken{};
    check(service.Subscribe("sample", &Events::Changed, &duplicate, &duplicateToken) == Status::Ok && duplicateToken != token,
        "duplicate subscriptions receive independent tokens");
    backend.DispatchChanges();
    check(events.keys.empty() && duplicate.keys == std::vector<std::string>{ "*" }, "new subscriber refresh does not refresh existing subscribers");
    check(service.Unsubscribe(duplicateToken) == Status::Ok && service.Unsubscribe(duplicateToken) == Status::UnknownSubscription &&
        service.Unsubscribe(0) == Status::UnknownSubscription, "unsubscribe removes exactly one token");
    API::Subscription unchanged = 123;
    check(service.Subscribe("BAD", &Events::Changed, &events, &unchanged) == Status::InvalidArgument && unchanged == 123 &&
        service.Subscribe("sample", nullptr, nullptr, &unchanged) == Status::InvalidArgument && unchanged == 123,
        "failed subscription leaves its output unchanged");
    Status missingStatus = Status::Ok;
    struct Missing { API::ISettings* service; Status* result; } missing{ &service, &missingStatus };
    API::Subscription missingToken{};
    check(service.Subscribe("missing", [](const char* mod, const char* key, void* context) noexcept {
        auto& self = *static_cast<Missing*>(context);
        bool value{};
        *self.result = key ? Status::InternalError : self.service->GetBool(mod, "enabled", &value);
    }, &missing, &missingToken) == Status::Ok, "unknown mods can subscribe");
    backend.DispatchChanges();
    check(missingStatus == Status::UnknownMod, "unknown mod still receives an initial refresh");
    service.Unsubscribe(missingToken);

    struct Reentrant
    {
        API::ISettings* service;
        SettingsService* backend;
        API::Subscription token{};
        int calls{};
        bool valid{ true };
    } reentrant{ &service, &backend };
    service.Subscribe("sample", [](const char* mod, const char*, void* context) noexcept {
        auto& self = *static_cast<Reentrant*>(context);
        ++self.calls;
        if (self.calls == 1) {
            self.valid = self.service->SetBool(mod, "enabled", true) == Status::Ok;
            self.backend->DispatchChanges();
            self.valid = self.valid && self.calls == 1;
        } else {
            self.valid = self.valid && self.service->Unsubscribe(self.token) == Status::Ok;
        }
    }, &reentrant, &reentrant.token);
    backend.DispatchChanges();
    check(reentrant.calls == 1 && reentrant.valid && backend.HasPendingChanges(), "callback writes defer and recursive dispatch is suppressed");
    service.SetInt("sample", "count", 7);
    backend.DispatchChanges();
    check(reentrant.calls == 2 && reentrant.valid && service.Unsubscribe(reentrant.token) == Status::UnknownSubscription,
        "self-unsubscribe suppresses remaining keys from the captured pass");
    events.keys.clear();

    std::atomic_int writeErrors{};
    std::jthread booleans([&] {
        for (int i = 0; i < 16; ++i) if (service.SetBool("sample", "enabled", i % 2 != 0) != Status::Ok) ++writeErrors;
    });
    std::jthread integers([&] {
        for (int i = 0; i < 16; ++i) if (service.SetInt("sample", "count", i % 10) != Status::Ok) ++writeErrors;
    });
    bool snapshotsValid = true;
    for (int i = 0; i < 32; ++i) {
        const auto snapshot = backend.Snapshot();
        snapshotsValid = snapshotsValid && snapshot.size() == 1 && snapshot[0].values.size() == 4;
    }
    booleans.join();
    integers.join();
    const auto concurrent = Json::parse(ReadFile(saved))["values"];
    check(writeErrors == 0 && snapshotsValid && concurrent["enabled"] == true && concurrent["count"] == 5,
        "concurrent writes and menu snapshots preserve both committed settings");
    check(events.keys.empty(), "worker writes do not invoke callbacks");
    backend.DispatchChanges();
    check(events.keys == std::vector<std::string>({ "count", "enabled" }) && events.valid, "concurrent notifications coalesce");
    service.Unsubscribe(token);

    struct Blocking
    {
        std::mutex mutex;
        std::condition_variable cv;
        bool entered{};
        bool release{};
        int calls{};
    } blocking;
    API::Subscription blockingToken{};
    service.Subscribe("sample", [](const char*, const char*, void* context) noexcept {
        auto& self = *static_cast<Blocking*>(context);
        std::unique_lock lock(self.mutex);
        ++self.calls;
        self.entered = true;
        self.cv.notify_all();
        self.cv.wait(lock, [&] { return self.release; });
    }, &blocking, &blockingToken);
    std::jthread dispatch([&] { backend.DispatchChanges(); });
    bool entered{};
    {
        std::unique_lock lock(blocking.mutex);
        entered = blocking.cv.wait_for(lock, 2s, [&] { return blocking.entered; });
    }
    std::promise<void> unsubscribing;
    auto finished = std::async(std::launch::async, [&] {
        unsubscribing.set_value();
        return service.Unsubscribe(blockingToken);
    });
    unsubscribing.get_future().wait();
    const bool waited = finished.wait_for(50ms) == std::future_status::timeout;
    backend.DispatchChanges(); // An overlapping pass must not invoke another callback.
    {
        std::lock_guard lock(blocking.mutex);
        blocking.release = true;
    }
    blocking.cv.notify_all();
    dispatch.join();
    check(entered && waited && finished.get() == Status::Ok && blocking.calls == 1,
        "external unsubscribe waits for an in-flight callback and dispatch remains serial");
    service.SetBool("sample", "enabled", false);
    backend.DispatchChanges();
    check(blocking.calls == 1, "no callbacks occur after unsubscribe returns");

    const auto owned = backend.GetValue("sample", "mode");
    check(owned && std::get<EnumValue>(*owned).value == "quiet" &&
        backend.SetValue("sample", "mode", EnumValue{ "verbose-mode" }) == SettingsError::None &&
        service.GetEnum("sample", "mode", text, sizeof(text), &required) == Status::Ok &&
        std::string_view(text) == "verbose-mode" && std::get<EnumValue>(*owned).value == "quiet",
        "internal reads own their values and internal writes reach the native adapter");
    check(service.SetEnum("sample", "mode", "quiet") == Status::Ok &&
        std::get<EnumValue>(backend.GetValue("sample", "mode").value()).value == "quiet",
        "native writes reach the internal service");

    SettingsService::Subscription internalToken{};
    int internalCalls{};
    bool internalValid = true;
    check(backend.Subscribe("sample", [&](const SettingsService::Change& change) noexcept {
        ++internalCalls;
        internalValid = internalValid && internalToken != 0 && change.mod == "sample" &&
            backend.GetValue(change.mod, "enabled").has_value();
        if (internalCalls == 1) {
            internalValid = internalValid && !change.key;
            return;
        }
        internalValid = internalValid && change.key && *change.key == "enabled";
    }, internalToken) == SettingsError::None, "internal subscriptions accept captured callbacks");
    backend.DispatchChanges();
    backend.SetValue("sample", "enabled", true);
    backend.DispatchChanges();
    const auto internalRemoved = std::async(std::launch::async, [&] { return backend.Unsubscribe(internalToken); }).get();
    check(internalCalls == 2 && internalValid && internalRemoved == SettingsError::None,
        "internal callbacks receive initial and later notifications and release the in-flight state");

    bool released{};
    auto capture = std::shared_ptr<int>(new int{}, [&](int* value) {
        delete value;
        released = backend.GetValue("sample", "enabled").has_value();
    });
    backend.Subscribe("sample", [capture](const SettingsService::Change&) {}, internalToken);
    capture.reset();
    check(backend.Unsubscribe(internalToken) == SettingsError::None && released,
        "callback captures are destroyed outside the service lock");

    int temporaryCalls{};
    API::Subscription temporaryToken{};
    {
        API::SettingsApi temporary{ backend, hotkeys };
        check(temporary.Subscribe("sample", [](const char*, const char*, void* context) noexcept {
            ++*static_cast<int*>(context);
        }, &temporaryCalls, &temporaryToken) == Status::Ok, "temporary adapter subscribes to the same service");
    }
    backend.DispatchChanges();
    check(temporaryCalls == 1 && service.Unsubscribe(temporaryToken) == Status::Ok,
        "subscriptions belong to the service and outlive their adapter");

    auto invalid = schema;
    invalid["groups"]["main"][0]["key"] = std::string("enabled\0suffix", 14);
    std::string error;
    check(!SettingsJson::ParseSchema(invalid, "sample", error) && error.find("NUL") != std::string::npos, "schema rejects keys truncated by the ABI");
    invalid = schema;
    invalid["groups"]["main"][3]["options"][1] = std::string("bad\0option", 10);
    check(!SettingsJson::ParseSchema(invalid, "sample", error) && error.find("NUL") != std::string::npos, "schema rejects enum values truncated by the ABI");
    return checks;
}

int main()
{
    try {
        const auto checks = TestSettingsService();
        std::cout << checks << " service checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
