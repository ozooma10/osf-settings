#include "Actions/ActionService.h"
#include "Papyrus/Actions.h"
#include "OSFSettings.h"
#include "Settings/SettingsJson.h"
#include "Settings/SettingsStore.h"
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <latch>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace OSFSettings
{
    std::optional<std::uint32_t> KeyCodeFromName(std::string_view) { throw std::runtime_error("action schema requested a native key"); }
    bool IsBindableKey(std::uint32_t) { throw std::runtime_error("action schema validated a native key"); }
}
extern "C" void* OSFSettings_RequestAPI(std::uint32_t, std::uint32_t*) noexcept;

int main()
{
    using namespace OSFSettings;
    using Json = nlohmann::ordered_json;
    using Error = ActionError;
    unsigned checks{};
    const auto check = [&](bool pass, const char* message) {
        if (!pass) throw std::runtime_error(message);
        ++checks;
    };
    try {
        const auto document = Json::parse(R"({"schemaVersion":1,"groups":{"Maintenance":[
            {"type":"action","id":"scan","label":"Rescan files","hint":"Reload the index","confirmation":"Rescan now?"},
            {"type":"action","id":"run","label":"Start scene"}]}})");
        std::string error;
        const auto schema = SettingsJson::ParseSchema(document, "actions", error);
        check(schema && schema->groups.size() == 1 && schema->groups[0].controls.size() == 2, "action-only group is retained");
        check(!schema->FindSetting("scan") && schema->FindAction("scan") && schema->FindAction("run"), "actions are not value definitions");
        check(schema->FindAction("scan")->confirmation == "Rescan now?" && schema->FindAction("run")->confirmation.empty(), "confirmation is optional and preserved");
        for (const auto* field : {"type", "id", "label", "hint", "confirmation"}) {
            auto bad = document; bad["groups"]["Maintenance"][0][field] = 42;
            check(!SettingsJson::ParseSchema(bad, "actions", error), "nontext action metadata rejected");
        }
        for (const auto* field : {"type", "id", "label"}) {
            auto bad = document; bad["groups"]["Maintenance"][0].erase(field);
            check(!SettingsJson::ParseSchema(bad, "actions", error), "required action fields cannot be omitted");
        }
        auto bad = document; bad["groups"]["Maintenance"][1]["id"] = "SCAN";
        check(!SettingsJson::ParseSchema(bad, "actions", error), "case-ambiguous action ids rejected");
        bad = document; bad["groups"]["Maintenance"][0]["confirmation"] = "";
        check(!SettingsJson::ParseSchema(bad, "actions", error), "explicit empty confirmation rejected");
        bad = document; bad["groups"]["Maintenance"][0]["confirmation"] = std::string("no\0yes", 6);
        check(!SettingsJson::ParseSchema(bad, "actions", error), "confirmation cannot hide text after a NUL");
        for (const auto* field : {"default", "value", "key", "requires", "group"}) {
            bad = document; bad["groups"]["Maintenance"][0][field] = "unused";
            check(!SettingsJson::ParseSchema(bad, "actions", error), "actions reject value metadata and redundant group placement");
        }
        bad = document; bad["groups"]["Maintenance"][0]["id"] = "../scan";
        check(!SettingsJson::ParseSchema(bad, "actions", error), "invalid identity rejected");
        bad = document; bad["groups"]["Maintenance"] = Json::object();
        check(!SettingsJson::ParseSchema(bad, "actions", error), "group controls must be an array");
        bad = document; bad["groups"]["Maintenance"][0] = 42;
        check(!SettingsJson::ParseSchema(bad, "actions", error), "group controls must be objects");
        bad = document; bad["actions"] = Json::array();
        check(!SettingsJson::ParseSchema(bad, "actions", error) && error.find("move each action into a group") != std::string::npos,
            "legacy top-level actions produce a migration error");
        auto grouped = document;
        grouped["groups"]["Gameplay"] = Json::array({grouped["groups"]["Maintenance"][1]});
        grouped["groups"]["Maintenance"].erase(1);
        const auto groups = SettingsJson::ParseSchema(grouped, "actions", error);
        check(groups && std::get<ActionDefinition>(groups->groups[0].controls[0]).id == "scan" &&
            std::get<ActionDefinition>(groups->groups[1].controls[0]).id == "run", "actions belong to their containing group");
        bad = grouped; bad["groups"]["Gameplay"][0]["id"] = "SCAN";
        check(!SettingsJson::ParseSchema(bad, "actions", error), "action ids are unique across groups ignoring case");

        auto mixed = grouped;
        mixed["groups"]["Maintenance"] = Json::array({
            {{"key", "enabled"}, {"type", "bool"}, {"default", true}},
            document["groups"]["Maintenance"][0],
            {{"key", "count"}, {"type", "int"}, {"default", 3}}
        });
        const auto ordered = SettingsJson::ParseSchema(mixed, "actions", error);
        check(ordered && ordered->groups[0].controls.size() == 3 &&
            std::get<SettingDefinition>(ordered->groups[0].controls[0]).key == "enabled" &&
            std::get<ActionDefinition>(ordered->groups[0].controls[1]).id == "scan" &&
            std::get<SettingDefinition>(ordered->groups[0].controls[2]).key == "count", "mixed controls preserve authored order");
        check(ordered->FindSetting("count") && ordered->FindAction("run") && !ordered->FindAction("enabled"), "typed lookups skip the other control kind");
        for (const auto* example : {"examples/actions/osfsettings-actions-example.json", "examples/papyrus/papyrusexample.json", "examples/localization/schemas/localization-example.json"}) {
            std::ifstream input(example);
            const auto parsed = SettingsJson::ParseSchema(input, std::filesystem::path(example).stem().string(), error);
            check(parsed && (parsed->FindAction("run") || parsed->FindAction("reset")), "shipped examples parse with grouped actions");
        }

        // Exercise the real value writer and resets with an action between settings.
        const auto root = std::filesystem::temp_directory_path() / ("osfsettings-actions-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); } } cleanup{root};
        std::filesystem::create_directories(root / "schemas");
        { std::ofstream file(root / "schemas/actions.json"); file << mixed; }
        SettingsStore store;
        store.LoadAll(root / "schemas", root / "values");
        check(store.LoadErrors().empty() && store.Set("actions", "enabled", false).ok, "ordinary setting remains writable with actions present");
        auto values = Json::parse(std::ifstream(root / "values/actions.json"));
        check(values["values"] == Json{{"count", 3}, {"enabled", false}}, "only ordinary settings are persisted");
        check(!store.Set("actions", "scan", true).ok && !store.Reset("actions", "scan").ok, "actions cannot be written or reset as settings");
        check(store.ResetMod("actions").ok, "mixed group resets its settings");
        values = Json::parse(std::ifstream(root / "values/actions.json"));
        check(values["values"] == Json{{"count", 3}, {"enabled", true}}, "mod reset preserves value-only persistence");
        store.LoadAll(root / "schemas", root / "values");
        check(store.LoadErrors().empty() && store.Mods()[0].schema.FindAction("scan"), "actions survive value reloads");

        ActionService service;
        const std::vector<ModSettings> mods{{*schema, {}}};
        std::uint64_t token = 999;
        check(service.Begin("actions", "scan", token) == Error::NotReady && token == 999, "not ready preserves output");
        service.Initialize(mods);
        check(!service.Status("actions", "scan").available, "unregistered action unavailable");
        check(service.Begin("actions", "scan", token) == Error::NotReady, "unregistered handler cannot run");
        unsigned calls{};
        auto callback = [&](auto invocation, const auto& mod, const auto& id) {
            check(mod == "actions" && id == "scan" && invocation != 0, "callback gets identity and nonzero token");
            ++calls;
        };
        check(service.Register("actions", "missing", callback) == Error::UnknownAction, "registration requires a declaration");
        check(service.Register("actions", "scan", callback) == Error::None, "native registration accepted");
        check(service.Register("actions", "scan", callback) == Error::AlreadyRegistered, "one handler, no fanout");
        check(service.Begin("actions", "scan", token) == Error::None && token != 999, "accepted invocation reserves a token");
        const auto first = token;
        check(service.Begin("actions", "scan", token) == Error::Busy, "repeat click rejected before callback runs");
        service.Dispatch(first); service.Dispatch(first);
        check(calls == 1 && service.Status("actions", "scan").state == ActionState::Running, "callback submits once and return is not completion");
        // Taking fresh snapshots models closing/reopening a presentation; no presentation owns this state.
        check(!service.Status("actions", "scan").available && service.Status("actions", "scan").message == "Working...", "pending status survives presentation reads");
        check(service.Complete(first, true, std::string("bad\0message", 11)) == Error::InvalidArgument, "malformed result preserves active invocation");
        auto completion = std::async(std::launch::async, [&] { return service.Complete(first, true, "Loaded 23 animations."); });
        check(completion.get() == Error::None, "completion accepted from another thread");
        check(service.Status("actions", "scan").available && service.Status("actions", "scan").message == "Loaded 23 animations.", "owned result survives callback return");
        check(service.Complete(first, false, "duplicate") == Error::UnknownInvocation, "first completion wins");
        check(service.Begin("actions", "scan", token) == Error::None && token != first, "next invocation gets a fresh token");
        check(service.Complete(first, false, "stale") == Error::UnknownInvocation && service.Status("actions", "scan").state == ActionState::Running, "old completion cannot finish new invocation");
        service.Complete(token, false, "Could not scan.");
        check(service.Status("actions", "scan").state == ActionState::Failed, "failure is shown and permits retry");

        std::latch start(3);
        auto begin = [&] { start.arrive_and_wait(); std::uint64_t out{}; return service.Begin("actions", "scan", out); };
        auto one = std::async(std::launch::async, begin), two = std::async(std::launch::async, begin);
        start.arrive_and_wait();
        const auto a = one.get(), b = two.get();
        check((a == Error::None && b == Error::Busy) || (b == Error::None && a == Error::Busy), "concurrent invocations atomically reserve one operation");
        service.ClearSession();
        check(service.Status("actions", "scan").available, "native handler survives session replacement");
        service.Begin("actions", "scan", token);
        service.Suspend(1); service.Suspend(2); service.Resume(1);
        check(service.Begin("actions", "run", token) == Error::NotReady, "overlapping lifecycle suspension remains closed");
        service.Dispatch(token);
        check(calls == 1 && service.Status("actions", "scan").state == ActionState::Failed, "undispatched work is not submitted during load");
        service.Resume(2);
        check(service.Status("actions", "scan").available, "failed load can resume existing registrations");
        service.Begin("actions", "scan", token);
        service.ClearSession(); service.Dispatch(token);
        check(calls == 1 && service.Complete(token, true, "") == Error::UnknownInvocation, "replaced session invalidates queued work and late completion");

        unsigned scriptCalls{};
        bool acceptScript = true;
        Papyrus::Actions scripts(service, [&](const Papyrus::Receiver&, auto invocation, const auto&, const auto&) {
            ++scriptCalls;
            if (acceptScript) service.Complete(invocation, true, "Script completed immediately.");
            return acceptScript;
        });
        const Papyrus::Receiver receiver{ 1234, "myscript" };
        check(scripts.Register(receiver, "actions", "run") == Error::None, "bound Papyrus registration");
        check(scripts.Register(receiver, "actions", "run") == Error::None, "same Papyrus registration is idempotent");
        check(scripts.Register({5678, "other"}, "actions", "run") == Error::AlreadyRegistered, "second Papyrus owner rejected");
        check(scripts.Register(receiver, "actions", "scan") == Error::AlreadyRegistered, "Papyrus cannot replace native owner");
        service.Begin("actions", "run", token); service.Dispatch(token);
        check(scriptCalls == 1 && service.Status("actions", "run").state == ActionState::Succeeded, "immediate completion works without a service-lock deadlock");
        acceptScript = false;
        service.Begin("actions", "run", token); service.Dispatch(token);
        check(service.Status("actions", "run").state == ActionState::Failed, "VM submission rejection completes as failure");
        scripts.ClearSession();
        check(!service.Status("actions", "run").available, "Papyrus owner is retired on load");
        check(scripts.Register({0, "global_script"}, "actions", "run") == Error::None, "Global receiver can register in new session");

        using ApiStatus = API::Status;
        API::Client client;
        check(client.CompleteAction(1, true) == ApiStatus::NotReady, "unattached public client is safe");
        std::uint32_t version = 99;
        check(!OSFSettings_RequestAPI(0x20000, &version) && version == 0, "unsupported ABI resets version output");
        auto* api = static_cast<API::ISettings*>(OSFSettings_RequestAPI(API::kVersion, &version));
        check(api && client.Attach(api, version), "settings export negotiates the shared interface");
        auto& exportedService = ActionService::Get(); exportedService.Initialize(mods);
        struct Context { unsigned calls{}; API::Client* client{}; } context{0, &client};
        const auto native = +[](std::uint64_t invocation, const char*, const char*, void* opaque) noexcept {
            auto& state = *static_cast<Context*>(opaque); ++state.calls;
            state.client->CompleteAction(invocation, true, "Native completed.");
        };
        check(client.RegisterAction(nullptr, "scan", native, &context) == ApiStatus::InvalidArgument, "null mod rejected at ABI");
        check(client.RegisterAction("actions", "scan", native, &context) == ApiStatus::Ok, "public registration accepted");
        check(client.RegisterAction("actions", "scan", native, &context) == ApiStatus::AlreadyRegistered, "duplicate ABI registration rejected");
        exportedService.Begin("actions", "scan", token); exportedService.Dispatch(token);
        check(context.calls == 1 && exportedService.Status("actions", "scan").message == "Native completed.", "public callback context and completion round trip");
        check(client.CompleteAction(token, true) == ApiStatus::UnknownInvocation, "ABI rejects duplicate completion");
        check(client.CompleteAction(0, true) == ApiStatus::InvalidArgument, "zero invocation rejected");
        std::cout << checks << " action checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
