#include "API/SettingsApi.h"
#include "OSFSettingsRegistry.h"
#include "Settings/SettingsService.h"
#include "RegistryConsumer.h"

#include <array>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <nlohmann/json.hpp>

extern "C" void* OSFSettings_RequestAPI(std::uint32_t, std::uint32_t*) noexcept;

namespace
{
    namespace API = OSFSettings::API;
    using API::Status;
    using TestJson = nlohmann::json;

    template <class... T>
    constexpr bool PublicRecords = ((std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>) && ...);
    static_assert(PublicRecords<API::TextView, API::RegistryValue, API::EnumOptionView,
        API::SettingView, API::GroupView, API::ModView, API::RegistryView>);
    static_assert(std::is_same_v<std::underlying_type_t<API::SettingType>, std::uint32_t>);
    static_assert(!std::is_destructible_v<API::ISettings>);

    std::string Text(API::TextView text) { return { text.data, text.size }; }

    TestJson Value(API::SettingType type, const API::RegistryValue& value)
    {
        switch (type) {
        case API::SettingType::Bool: return value.boolean;
        case API::SettingType::Int: return value.integer;
        case API::SettingType::Float: return value.number;
        case API::SettingType::Enum:
        case API::SettingType::String: return Text(value.text);
        case API::SettingType::Key: return value.key;
        }
        throw std::runtime_error("unknown public setting type");
    }

    struct Capture
    {
        TestJson mods = TestJson::array();
        unsigned calls{};
        bool copied{};
        bool validArrays{ true };
        std::thread::id thread;

        static void OnRegistry(const API::RegistryView& registry, void* context) noexcept
        {
            auto& self = *static_cast<Capture*>(context);
            ++self.calls;
            self.thread = std::this_thread::get_id();
            try {
                self.validArrays &= (registry.mods != nullptr) == (registry.modCount != 0);
                for (std::uint32_t m = 0; m < registry.modCount; ++m) {
                    const auto& mod = registry.mods[m];
                    self.validArrays &= (mod.groups != nullptr) == (mod.groupCount != 0);
                    TestJson groups = TestJson::array();
                    for (std::uint32_t g = 0; g < mod.groupCount; ++g) {
                        const auto& group = mod.groups[g];
                        self.validArrays &= (group.settings != nullptr) == (group.settingCount != 0);
                        TestJson settings = TestJson::array();
                        for (std::uint32_t s = 0; s < group.settingCount; ++s) {
                            const auto& setting = group.settings[s];
                            self.validArrays &= (setting.options != nullptr) == (setting.optionCount != 0);
                            TestJson item{
                                {"key", Text(setting.key)}, {"label", Text(setting.label)}, {"hint", Text(setting.hint)},
                                {"type", setting.type}, {"requiresRestart", setting.requiresRestart},
                                {"value", Value(setting.type, setting.value)}, {"default", Value(setting.type, setting.defaultValue)}
                            };
                            if (setting.hasMinimum) item["min"] = Value(setting.type, setting.minimum);
                            if (setting.hasMaximum) item["max"] = Value(setting.type, setting.maximum);
                            if (setting.type == API::SettingType::Float) item["step"] = setting.step;
                            if (setting.type == API::SettingType::String) item["maxLength"] = setting.maxLength;
                            if (setting.type == API::SettingType::Key) item["allowUnbound"] = setting.allowUnbound;
                            if (setting.type == API::SettingType::Enum) {
                                item["options"] = TestJson::array();
                                for (std::uint32_t o = 0; o < setting.optionCount; ++o) {
                                    item["options"].push_back({{"value", Text(setting.options[o].value)}, {"label", Text(setting.options[o].label)}});
                                }
                            }
                            settings.push_back(std::move(item));
                        }
                        groups.push_back({{"id", Text(group.id)}, {"label", Text(group.label)}, {"settings", std::move(settings)}});
                    }
                    self.mods.push_back({{"id", Text(mod.id)}, {"title", Text(mod.title)},
                        {"description", Text(mod.description)}, {"groups", std::move(groups)}});
                }
                self.copied = true;
            } catch (...) {}
        }
    };

    Capture Read(API::Client& client, const char* mod = nullptr)
    {
        Capture capture;
        if (client.ReadRegistry(mod, Capture::OnRegistry, &capture) != Status::Ok || !capture.copied || !capture.validArrays) {
            throw std::runtime_error("registry capture failed");
        }
        return capture;
    }

    struct Notifications
    {
        API::Client* client;
        unsigned calls{};
        unsigned full{};
        std::vector<std::string> keys;
        Capture latest;
        Status status{ Status::NotReady };
        static void Changed(const char* mod, const char* key, void* context) noexcept
        {
            auto& self = *static_cast<Notifications*>(context);
            ++self.calls;
            try {
                if (key) self.keys.emplace_back(key);
                else ++self.full;
                self.latest = {};
                self.status = self.client->ReadRegistry(mod, Capture::OnRegistry, &self.latest);
            } catch (...) { self.status = Status::InternalError; }
        }
    };

    void Write(const std::filesystem::path& path, const TestJson& value)
    {
        std::ofstream output(path, std::ios::binary);
        output << value.dump();
        if (!output) throw std::runtime_error("cannot write fixture");
    }
}

int main()
{
    unsigned checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    try {
        const auto root = std::filesystem::temp_directory_path() /
            ("osfsettings-registry-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup
        {
            std::filesystem::path root;
            ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
        } cleanup{ root };
        const auto schemas = root / "schemas", values = root / "values";
        std::filesystem::create_directories(schemas);
        std::filesystem::create_directories(values);
        auto schema = TestJson::parse(R"({
            "schemaVersion":1,"id":"alpha","title":"Alpha title","description":"Description",
            "groups":[{"id":"second","label":"First group","settings":[
                {"key":"enabled","type":"bool","label":"Enabled","hint":"A hint","requires":"restart","default":false},
                {"key":"count","type":"int","default":-9007199254740993,"min":-9223372036854775808,"max":9223372036854775807},
                {"key":"scale","type":"float","default":0.15,"min":-1.25,"max":2.5,"step":0.125},
                {"key":"mode","type":"enum","default":"quiet","options":["verbose","quiet"],"optionLabels":["Verbose label","Quiet label"]},
                {"key":"key","type":"key","default":"UNBOUND","allowUnbound":true},
                {"key":"text","type":"string","default":"Original text","maxLength":32}
            ]},{"id":"first","settings":[]},{"id":"extra","settings":[
                {"key":"unbounded","type":"int","default":0},
                {"key":"lower","type":"float","default":0,"min":-1},
                {"key":"upper","type":"int","default":0,"max":1},
                {"key":"empty","type":"string","default":""},
                {"key":"bound","type":"key","default":115}
            ]}]
        })");
        const std::string embedded("prefix\0suffix", 13);
        const std::string unicode = "\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80";
        schema["title"] = embedded;
        schema["description"] = unicode;
        schema["groups"][0]["settings"][0]["label"] = embedded;
        schema["groups"][0]["settings"][0]["hint"] = unicode;
        schema["groups"][0]["settings"][3]["optionLabels"][1] = embedded;
        schema["groups"][1]["id"] = embedded;
        schema["groups"][1]["label"] = unicode;
        Write(schemas / "alpha.json", schema);
        Write(schemas / "beta.json", TestJson::parse(R"({"schemaVersion":1,"id":"beta","groups":[]})"));
        Write(schemas / "hotkeys.json", TestJson::parse(R"({"schemaVersion":1,"id":"hotkeys","groups":[],"hotkeys":[{"id":"open","label":"Open","default":"F10"}]})"));
        Write(values / "alpha.json", {{"formatVersion",1},{"values",{{"enabled",true},{"count",9007199254740993LL},{"scale",1.75},{"mode","verbose"},{"key",65},{"text",unicode}}}});

        OSFSettings::SettingsService backend;
        API::SettingsApi adapter(backend);
        API::Client client;
        Capture errors;
        check(client.ReadRegistry(nullptr, Capture::OnRegistry, &errors) == Status::NotReady && errors.calls == 0, "detached client");
        check(client.Attach(&adapter), "attach provider");
        check(client.ReadRegistry(nullptr, nullptr, nullptr) == Status::InvalidArgument, "missing callback");
        for (const char* mod : {"", "BAD", ".", "..", "path/mod"}) {
            check(client.ReadRegistry(mod, Capture::OnRegistry, &errors) == Status::InvalidArgument, "invalid mod before readiness");
        }
        check(client.ReadRegistry(nullptr, Capture::OnRegistry, &errors) == Status::NotReady && errors.calls == 0, "unloaded service");
        backend.Load(schemas, values);
        check(backend.LoadErrors().empty(), "fixtures load without errors");
        check(client.ReadRegistry("alpha", Capture::OnRegistry, &errors) == Status::NotReady, "loaded but not started");
        Notifications notifications{ &client };
        API::Subscription token{};
        check(client.Subscribe("alpha", Notifications::Changed, &notifications, &token) == Status::Ok, "subscribe before readiness");
        backend.DispatchChanges();
        check(notifications.calls == 0, "initial notification waits for readiness");
        backend.Start();
        check(client.ReadRegistry("missing", Capture::OnRegistry, &errors) == Status::UnknownMod && errors.calls == 0, "unknown filtered mod has no callback");
        check(client.ReadRegistry("alpha", [](const API::RegistryView&, void*) noexcept {}, nullptr) == Status::Ok, "null context allowed");

        const auto all = Read(client);
        check(all.calls == 1 && all.thread == std::this_thread::get_id() && all.mods.size() == 3, "one synchronous whole-registry callback");
        check(all.mods[0]["id"] == "alpha" && all.mods[1]["id"] == "beta" && all.mods[2]["id"] == "hotkeys", "mod identities");
        const auto selected = Read(client, "alpha");
        check(selected.mods.size() == 1 && selected.mods[0] == all.mods[0], "exact mod filter");
        check(all.mods[1]["groups"].empty() && all.mods[1]["title"] == "beta", "empty mods and parsed fallback labels");
        const auto& hotkeyGroups = all.mods[2]["groups"];
        check(hotkeyGroups.size() == 1 && hotkeyGroups[0]["settings"].empty(), "native hotkeys do not become setting values");
        const auto& mod = all.mods[0];
        const auto& groups = mod["groups"];
        const auto& settings = groups[0]["settings"];
        check(mod["title"] == embedded && mod["description"] == unicode && groups[1]["id"] == embedded && groups[1]["label"] == unicode,
            "length-aware mod and group text");
        check(groups[0]["id"] == "second" && groups[2]["id"] == "extra" && groups[1]["settings"].empty(), "group order and empty groups");
        const std::array types{API::SettingType::Bool, API::SettingType::Int, API::SettingType::Float, API::SettingType::Enum, API::SettingType::Key, API::SettingType::String};
        const std::array keys{"enabled", "count", "scale", "mode", "key", "text"};
        for (std::size_t i = 0; i < types.size(); ++i) check(settings[i]["type"] == types[i] && settings[i]["key"] == keys[i], "setting identity, type and authored order");
        check(settings[0]["value"] == true && settings[0]["default"] == false && settings[0]["requiresRestart"] == true &&
            settings[0]["label"] == embedded && settings[0]["hint"] == unicode, "bool metadata and distinct persisted value");
        check(settings[1]["value"].get<std::int64_t>() == 9007199254740993LL && settings[1]["default"].get<std::int64_t>() == -9007199254740993LL &&
            settings[1]["min"].get<std::int64_t>() == INT64_MIN && settings[1]["max"].get<std::int64_t>() == INT64_MAX, "integer precision and bounds");
        check(settings[2]["value"] == 1.75 && settings[2]["default"] == 0.15 && settings[2]["min"] == -1.25 &&
            settings[2]["max"] == 2.5 && settings[2]["step"] == 0.125, "float metadata and precision");
        check(settings[3]["value"] == "verbose" && settings[3]["default"] == "quiet" &&
            settings[3]["options"][0]["value"] == "verbose" && settings[3]["options"][1]["value"] == "quiet" &&
            settings[3]["options"][0]["label"] == "Verbose label" && settings[3]["options"][1]["label"] == embedded, "enum identifiers, labels and option order");
        check(settings[4]["value"] == 65 && settings[4]["default"] == API::kUnboundKey && settings[4]["allowUnbound"] == true, "resolved native key values");
        check(settings[5]["value"] == unicode && settings[5]["default"] == "Original text" && settings[5]["maxLength"] == 32, "string current/default bytes and limit");
        const auto& extra = groups[2]["settings"];
        check(!extra[0].contains("min") && !extra[0].contains("max") && extra[1]["min"] == -1 && !extra[1].contains("max") &&
            !extra[2].contains("min") && extra[2]["max"] == 1, "absent and one-sided bounds");
        check(extra[1]["step"] == 0.1 && extra[3]["value"] == "" && extra[3]["maxLength"] == 256 &&
            extra[4]["value"] == 115 && extra[4]["allowUnbound"] == false, "parsed metadata defaults");
        check(!settings[0].contains("min") && !settings[0].contains("max") && !settings[0].contains("options") && !settings[1]["requiresRestart"].get<bool>(), "type-specific metadata stays absent");

        backend.DispatchChanges();
        check(notifications.calls == 1 && notifications.full == 1 && notifications.keys.empty() && notifications.status == Status::Ok &&
            notifications.latest.mods == selected.mods, "initial null-key callback enumerates every current value");
        RegistryExample::RegistryConsumer consumer(client);
        check(consumer.Start() == Status::Ok, "SDK-only example starts");
        const auto owned = consumer.Snapshot();
        check(owned.size() == 3 && std::get<std::string>(owned.at("alpha").at("text").value) == unicode &&
            owned.at("alpha").at("mode").type == API::SettingType::Enum && owned.at("beta").empty(), "example retains owned typed values and empty mods");
        backend.DispatchChanges();
        check(consumer.LastRefresh() == Status::Ok, "example handles initial null notifications");

        struct Reentrant
        {
            API::Client* client;
            Status write{};
            Capture nested;
            bool stable{};
            static void OnRegistry(const API::RegistryView& registry, void* context) noexcept
            {
                auto& self = *static_cast<Reentrant*>(context);
                const auto& text = registry.mods[0].groups[0].settings[5];
                const auto before = text.value.text;
                self.write = self.client->SetString("alpha", "text", "changed inside callback");
                self.client->ReadRegistry("alpha", Capture::OnRegistry, &self.nested);
                self.stable = std::string_view(before.data, before.size) == "\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80" &&
                    std::string_view(text.defaultValue.text.data, text.defaultValue.text.size) == "Original text";
            }
        } reentrant{ &client };
        check(client.ReadRegistry(nullptr, Reentrant::OnRegistry, &reentrant) == Status::Ok && reentrant.write == Status::Ok && reentrant.stable &&
            reentrant.nested.mods[0]["groups"][0]["settings"][5]["value"] == "changed inside callback", "unlocked reentrant writes and nested reads preserve outer snapshot");
        check(settings[5]["value"] == unicode && std::get<std::string>(owned.at("alpha").at("text").value) == unicode, "copied data survives return and later writes");
        check(client.SetString("alpha", "text", "second") == Status::Ok && client.SetString("alpha", "text", "latest") == Status::Ok, "repeated writes");
        backend.DispatchChanges();
        check(notifications.calls == 2 && notifications.keys == std::vector<std::string>{"text"} && notifications.full == 1 &&
            notifications.latest.mods[0]["groups"][0]["settings"][5]["value"] == "latest", "keyed changes coalesce and refresh current values");
        check(std::get<std::string>(consumer.Snapshot().at("alpha").at("text").value) == "latest", "example refreshes keyed notifications");

        std::filesystem::create_directory(values / "alpha.json.tmp");
        check(client.SetString("alpha", "text", "failed") == Status::SaveFailed && client.ResetMod("alpha") == Status::SaveFailed && !backend.HasPendingChanges(), "failed persistence creates no invalidation");
        check(Read(client, "alpha").mods[0]["groups"][0]["settings"][5]["value"] == "latest", "failed persistence never enters registry");
        backend.DispatchChanges();
        check(notifications.calls == 2, "no callbacks after failed persistence");
        std::filesystem::remove(values / "alpha.json.tmp");
        check(client.ResetMod("alpha") == Status::Ok, "whole mod reset");
        backend.DispatchChanges();
        check(notifications.calls == 3 && notifications.full == 2 && notifications.latest.mods[0]["groups"][0]["settings"][5]["value"] == "Original text" &&
            std::get<bool>(consumer.Snapshot().at("alpha").at("enabled").value) == false, "null reset notification refreshes all settings and example");
        check(client.ResetMod("alpha") == Status::Ok && !backend.HasPendingChanges(), "unchanged reset stays silent");

        // A paused callback must not hold the service lock. Another thread can
        // change values while both the old snapshot and a new read remain valid.
        struct Concurrent
        {
            std::promise<void> entered;
            std::shared_future<void> release;
            bool stable{};
            std::thread::id thread;
            static void OnRegistry(const API::RegistryView& registry, void* context) noexcept
            {
                auto& self = *static_cast<Concurrent*>(context);
                self.thread = std::this_thread::get_id();
                self.entered.set_value();
                self.release.wait();
                const auto& entries = registry.mods[0].groups[0].settings;
                self.stable = entries[0].value.boolean == false && entries[1].value.integer == -9007199254740993LL &&
                    registry.modCount == 3 && entries[5].defaultValue.text.size == 13;
            }
        } concurrent;
        std::promise<void> release;
        concurrent.release = release.get_future().share();
        auto reading = std::async(std::launch::async, [&] { return client.ReadRegistry(nullptr, Concurrent::OnRegistry, &concurrent); });
        concurrent.entered.get_future().wait();
        auto writing = std::async(std::launch::async, [&] { return client.SetBool("alpha", "enabled", true); });
        const bool unlocked = writing.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        release.set_value(); // Release even on failure, so a regression does not hang the test.
        check(reading.get() == Status::Ok && writing.get() == Status::Ok && unlocked && concurrent.stable &&
            concurrent.thread != std::this_thread::get_id(), "concurrent writer completes while snapshot callback is blocked");

        std::atomic_bool coherent{ true };
        std::jthread writer([&] {
            for (int i = 0; i < 12; ++i) {
                if (client.SetBool("alpha", "enabled", true) != Status::Ok || client.SetString("alpha", "text", "updated") != Status::Ok ||
                    client.ResetMod("alpha") != Status::Ok) coherent = false;
            }
        });
        for (int i = 0; i < 30; ++i) {
            const auto current = Read(client);
            const auto& first = current.mods[0]["groups"][0]["settings"];
            // The writer changes bool, then text, then resets both atomically.
            if (first[0]["value"] == false && first[5]["value"] != "Original text") coherent = false;
        }
        writer.join();
        check(coherent, "concurrent captures never split an atomic reset");
        backend.DispatchChanges();
        check(consumer.LastRefresh() == Status::Ok && std::get<bool>(consumer.Snapshot().at("alpha").at("enabled").value) == false,
            "example converges after concurrent commits and resets");

        const auto callsBeforeStop = notifications.calls;
        check(client.Unsubscribe(token) == Status::Ok && client.Unsubscribe(token) == Status::UnknownSubscription, "unsubscribe token contract");
        consumer.Stop();
        check(client.SetString("alpha", "text", "after stop") == Status::Ok && !backend.HasPendingChanges(), "stopped consumers receive no invalidations");
        backend.DispatchChanges();
        check(notifications.calls == callsBeforeStop && std::get<std::string>(consumer.Snapshot().at("alpha").at("text").value) == "Original text", "unsubscribe preserves last owned copies");

        const auto emptySchemas = root / "empty";
        std::filesystem::create_directory(emptySchemas);
        OSFSettings::SettingsService emptyBackend;
        emptyBackend.Load(emptySchemas, root / "empty-values");
        emptyBackend.Start();
        API::SettingsApi emptyAdapter(emptyBackend);
        API::Client emptyClient;
        emptyClient.Attach(&emptyAdapter);
        const auto empty = Read(emptyClient);
        check(empty.calls == 1 && empty.mods.empty() && empty.validArrays, "empty registry still invokes once");
        std::uint32_t version{};
        auto* exported = static_cast<API::ISettings*>(OSFSettings_RequestAPI(API::kVersion, &version));
        check(exported && version == API::kVersion &&
            !OSFSettings_RequestAPI(API::kVersion + 1, nullptr), "existing pre-launch ABI negotiation");
        check(exported->ReadRegistry(nullptr, Capture::OnRegistry, &errors) == Status::NotReady && errors.calls == 0,
            "exported interface dispatches the appended registry slot");
        std::cout << checks << " registry API checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
