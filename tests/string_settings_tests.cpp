#include "FileLock.h"
#include "API/SettingsApi.h"
#include "Input/HotkeyInputState.h"
#include "Settings/SettingsJson.h"
#include "Settings/SettingsService.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>

extern "C" void* OSFSettings_RequestAPI(std::uint32_t, std::uint32_t*) noexcept;

namespace
{
    using namespace OSFSettings;
    using API::Status;
    using TestJson = nlohmann::ordered_json;

    std::string Read(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    }

    struct Events
    {
        API::Client* client;
        std::filesystem::path saved;
        std::vector<std::string> keys;
        bool savedBeforePublish{ true };
        static void Changed(const char* mod, const char* key, void* context) noexcept
        {
            auto& self = *static_cast<Events*>(context);
            self.keys.emplace_back(key ? key : "*");
            std::string value;
            if (std::filesystem::exists(self.saved)) {
                self.savedBeforePublish &= self.client->GetString(mod, "language", value) == Status::Ok &&
                    TestJson::parse(Read(self.saved))["values"]["language"] == value;
            }
        }
    };
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
            ("osfsettings-strings-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup
        {
            std::filesystem::path root;
            ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
        } cleanup{ root };
        const auto schemas = root / "schemas", values = root / "values", saved = values / "osfui.json";
        std::filesystem::create_directories(schemas);
        auto schema = TestJson::parse(Read("tests/fixtures/osfui-language.json"));
        schema["groups"]["Interface"].push_back({{"key", "mode"}, {"type", "enum"}, {"default", "auto"}, {"options", {"auto", "en"}}});
        schema["groups"]["Interface"].push_back({{"key", "enabled"}, {"type", "bool"}, {"default", false}});
        schema["groups"]["Interface"].push_back({{"key", "text"}, {"type", "string"}, {"default", ""}, {"maxLength", 4096}});
        std::string error;
        const auto parsed = SettingsJson::ParseSchema(schema, "osfui", error);
        check(parsed && error.empty(), "OSF UI language declaration parses in a Slim schema");
        const auto& definition = std::get<StringDefinition>(parsed->FindSetting("language")->definition);
        check(definition.maxLength == 32 && definition.defaultValue == "auto", "language stays free-form with authored default and limit");

        auto changed = schema;
        changed["groups"]["Interface"][0].erase("maxLength");
        check(std::get<StringDefinition>(SettingsJson::ParseSchema(changed, "osfui", error)->FindSetting("language")->definition).maxLength == 256,
            "omitted maxLength is 256 UTF-8 bytes");
        for (const TestJson& limit : {TestJson(0), TestJson(-1), TestJson(4097), TestJson(32.0), TestJson(true), TestJson(nullptr), TestJson("32"), TestJson(UINT64_MAX)}) {
            changed = schema; changed["groups"]["Interface"][0]["maxLength"] = limit;
            check(!SettingsJson::ParseSchema(changed, "osfui", error), "invalid maxLength is a schema error");
        }
        for (const auto limit : {1, 4096}) {
            changed = schema;
            changed["groups"]["Interface"][0]["maxLength"] = limit;
            changed["groups"]["Interface"][0]["default"] = std::string(limit, 'x');
            check(SettingsJson::ParseSchema(changed, "osfui", error).has_value(), "inclusive schema byte boundaries");
        }
        changed = schema; changed["groups"]["Interface"][0].erase("default");
        check(!SettingsJson::ParseSchema(changed, "osfui", error), "string default is required");
        changed["groups"]["Interface"][0]["default"] = "";
        check(SettingsJson::ParseSchema(changed, "osfui", error).has_value(), "empty default is valid");
        changed["groups"]["Interface"][0]["default"] = true;
        check(!SettingsJson::ParseSchema(changed, "osfui", error), "default is not coerced to text");

        const std::vector<std::string> invalid{
            std::string(33, 'x'), std::string("a\0b", 3), "a\tb", "a\nb", "a\rb", "\x7F", "\xC2\x80", "\xC2\x9F",
            "\xE2\x80\xA8", "\xE2\x80\xA9", "\x80", "\xC0\xAF", "\xE0\x80\xAF", "\xF0\x80\x80\xAF",
            "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\xFF", "\xC2", "\xE2\x82", "\xF0\x9F\x98", "\xC2x"
        };
        for (const auto& text : invalid) {
            changed = schema; changed["groups"]["Interface"][0]["default"] = text;
            check(!IsValidString(text, 32) && !SettingsJson::ParseSchema(changed, "osfui", error), "schema and value validation reject the same text");
        }
        const std::string unicode = "\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80"; // 9 bytes, 3 scalars.
        check(IsValidString(unicode, 9) && !IsValidString(unicode, 8), "length counts UTF-8 bytes rather than characters or UTF-16 units");
        check(IsValidString("\xF4\x8F\xBF\xBF", 4) && IsValidString("\xC2\xA0", 2), "valid Unicode bounds and non-control spaces are accepted");
        { std::ofstream output(schemas / "osfui.json"); output << schema.dump(); }

        HotkeyInputState input;
        SettingsService backend;
        API::SettingsApi api(backend, input);
        API::Client client;
        std::string owned = "unchanged";
        check(client.GetString("osfui", "language", owned) == Status::NotReady && owned == "unchanged", "disconnected SDK preserves owned output");
        check(client.Attach(&api), "attach current initial ABI");
        char buffer[64] = "sentinel";
        std::uint32_t required = 99;
        check(client.GetString("osfui", "language", buffer, sizeof(buffer), &required) == Status::NotReady && required == 99 && std::string(buffer) == "sentinel",
            "unready getter preserves both outputs");
        check(client.SetString("osfui", "language", "en") == Status::NotReady, "unready write cannot publish");
        backend.Load(schemas, values);
        backend.Start();
        check(backend.LoadErrors().empty(), "fixture loads without diagnostics");
        Events events{ &client, saved };
        API::Subscription token{};
        check(client.Subscribe("osfui", &Events::Changed, &events, &token) == Status::Ok, "string consumer subscribes before reading");
        backend.DispatchChanges();
        check(events.keys == std::vector<std::string>{"*"}, "initial invalidation");
        events.keys.clear();

        check(client.GetString("osfui", "language", owned) == Status::Ok && owned == "auto", "owned string getter returns auto");
        check(client.GetString("osfui", "language", nullptr, 0, &required) == Status::BufferTooSmall && required == 5,
            "size query includes final NUL");
        check(client.GetString("osfui", "language", buffer, 4, &required) == Status::BufferTooSmall && required == 5 && std::string(buffer) == "sentinel",
            "short output is never truncated or modified");
        check(client.GetString("osfui", "language", buffer, 5, &required) == Status::Ok && required == 5 && std::string(buffer) == "auto", "exact buffer copy");
        required = 99;
        check(client.GetString("osfui", "language", nullptr, 1, &required) == Status::InvalidArgument && required == 99 &&
            client.GetString("osfui", "language", buffer, sizeof(buffer), nullptr) == Status::InvalidArgument, "invalid buffer arguments");
        check(client.GetString(nullptr, "language", buffer, sizeof(buffer), &required) == Status::InvalidArgument &&
            client.GetString("BAD", "language", buffer, sizeof(buffer), &required) == Status::InvalidArgument &&
            client.GetString("missing", "language", buffer, sizeof(buffer), &required) == Status::UnknownMod &&
            client.GetString("osfui", "missing", buffer, sizeof(buffer), &required) == Status::UnknownSetting &&
            required == 99 && std::string(buffer) == "auto", "lookup errors preserve outputs");
        check(client.GetString("osfui", "mode", owned) == Status::TypeMismatch && owned == "auto" &&
            client.GetEnum("osfui", "language", owned) == Status::TypeMismatch && owned == "auto" &&
            client.SetString("osfui", "mode", "en") == Status::TypeMismatch &&
            client.SetEnum("osfui", "language", "en") == Status::TypeMismatch &&
            client.SetBool("osfui", "language", false) == Status::TypeMismatch &&
            client.SetString("osfui", "enabled", "true") == Status::TypeMismatch, "enum, scalar and string types remain distinct");
        check(client.SetEnum("osfui", "mode", "pt-BR") == Status::InvalidValue, "enum remains limited to authored options");
        check(client.SetString("osfui", "language", nullptr, 0) == Status::InvalidArgument &&
            client.SetString("osfui", "language", nullptr) == Status::InvalidArgument, "null text is not empty text");
        check(client.SetString("osfui", "language", "auto") == Status::Ok && !backend.HasPendingChanges() && !std::filesystem::exists(saved),
            "unchanged string does not save or notify");

        for (const auto& text : {std::string("pt-BR"), std::string("zz-Latn-ZZ-x-custom"), std::string("  PT_br  "), unicode, std::string(32, 'z'), std::string()}) {
            check(client.SetString("osfui", "language", std::string_view(text)) == Status::Ok, "arbitrary locales and valid free-form strings save");
            check(client.GetString("osfui", "language", buffer, sizeof(buffer), &required) == Status::Ok &&
                required == text.size() + 1 && std::string(buffer) == text && TestJson::parse(Read(saved))["values"]["language"] == text,
                "saved bytes, caller buffer and disk agree without normalization");
            SettingsStore reloaded; reloaded.LoadAll(schemas, values);
            check(reloaded.LoadErrors().empty() && std::get<std::string>(*reloaded.GetValue("osfui", "language")) == text,
                "string reload preserves exact text");
        }
        check(owned == "auto" && events.keys.empty(), "owned result is independent and changes are not dispatched inline");
        backend.DispatchChanges();
        check(events.keys == std::vector<std::string>{"language"} && events.savedBeforePublish, "changes coalesce after persistence");
        events.keys.clear();
        check(client.GetString("osfui", "language", nullptr, 0, &required) == Status::BufferTooSmall && required == 1, "empty text still requires a terminator");
        const char raw[]{'d', 'e'};
        check(client.SetString("osfui", "language", raw, 2) == Status::Ok, "explicit setter accepts unterminated caller input");
        client.GetString("osfui", "language", nullptr, 0, &required);
        check(required == 3 && client.SetString("osfui", "language", "zh-Hant-TW") == Status::Ok &&
            client.GetString("osfui", "language", buffer, required, &required) == Status::BufferTooSmall && required == 11 &&
            client.GetString("osfui", "language", owned) == Status::Ok && owned == "zh-Hant-TW", "caller can retry when text grows between query and copy");
        backend.DispatchChanges(); events.keys.clear();

        const auto persisted = Read(saved);
        for (const auto& text : invalid) {
            check(client.SetString("osfui", "language", std::string_view(text)) == Status::InvalidValue &&
                backend.SetValue("osfui", "language", text) == SettingsError::InvalidValue,
                "API and service reject the schema validation corpus");
        }
        check(client.SetString("osfui", "language", "", 4097) == Status::InvalidValue, "global cap rejects before copying caller bytes");
        check(Read(saved) == persisted && !backend.HasPendingChanges(), "invalid writes leave storage and notifications unchanged");
        OSFSettings::Test::FileLock writeLock(saved);
        check(client.SetString("osfui", "language", "en") == Status::SaveFailed &&
            client.Reset("osfui", "language") == Status::SaveFailed && client.ResetMod("osfui") == Status::SaveFailed, "save failures propagate through strings and resets");
        check(client.GetString("osfui", "language", owned) == Status::Ok && owned == "zh-Hant-TW" &&
            Read(saved) == persisted && !backend.HasPendingChanges(), "failed save preserves published string, disk and notifications");
        check(client.SetString("osfui", "language", owned) == Status::Ok && !backend.HasPendingChanges(), "equal string bypasses unavailable storage");
        writeLock.Release();
        check(client.Reset("osfui", "language") == Status::Ok && TestJson::parse(Read(saved))["values"]["language"] == "auto", "single reset persists authored string default");
        backend.DispatchChanges();
        check(events.keys == std::vector<std::string>{"language"}, "single reset notifies string key");
        events.keys.clear();
        check(client.Reset("osfui", "language") == Status::Ok && !backend.HasPendingChanges(), "unchanged reset is silent");
        client.SetString("osfui", "language", "de"); client.SetEnum("osfui", "mode", "en");
        check(client.ResetMod("osfui") == Status::Ok && TestJson::parse(Read(saved))["values"]["language"] == "auto" &&
            TestJson::parse(Read(saved))["values"]["mode"] == "auto", "mod reset restores strings and enums atomically");
        backend.DispatchChanges();
        check(events.keys == std::vector<std::string>{"*"} && events.savedBeforePublish, "mod reset publishes full invalidation after save");
        client.Unsubscribe(token);

        const std::string maximum(4096, 'x');
        check(client.SetString("osfui", "text", maximum) == Status::Ok && client.GetString("osfui", "text", owned) == Status::Ok && owned == maximum,
            "API and owned getter accept the full global limit");
        SettingsStore maximumReload; maximumReload.LoadAll(schemas, values);
        check(std::get<std::string>(*maximumReload.GetValue("osfui", "text")) == maximum, "maximum-length value persists and reloads");
        check(client.SetString("osfui", "text", maximum + "x") == Status::InvalidValue, "global limit plus one is rejected");
        check(client.SetString("osfui", "text", std::string_view{}) == Status::Ok && client.GetString("osfui", "text", owned) == Status::Ok && owned.empty(),
            "empty default view is a valid empty string");

        for (const auto& text : {TestJson(42), TestJson(std::string(33, 'x')), TestJson("a\nb"), TestJson(std::string("a\0b", 3))}) {
            { std::ofstream output(saved); output << TestJson{{"formatVersion", 1}, {"values", {{"language", text}, {"mode", "en"}}}}.dump(); }
            SettingsStore reloaded; reloaded.LoadAll(schemas, values);
            check(reloaded.LoadErrors().size() == 1 && std::get<std::string>(*reloaded.GetValue("osfui", "language")) == "auto" &&
                std::get<EnumValue>(*reloaded.GetValue("osfui", "mode")).value == "en", "invalid persisted text falls back to default without discarding valid siblings");
        }
        { std::ofstream output(saved, std::ios::binary); output << "{\"formatVersion\":1,\"values\":{\"language\":\"\xC0\xAF\"}}"; }
        SettingsStore reloaded; reloaded.LoadAll(schemas, values);
        check(!reloaded.LoadErrors().empty() && std::get<std::string>(*reloaded.GetValue("osfui", "language")) == "auto", "malformed UTF-8 file keeps defaults with a diagnostic");
        std::uint32_t version{};
        check(OSFSettings_RequestAPI(API::kVersion, &version) && version == API::kVersion, "export serves the current initial contract");
        std::cout << checks << " string checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
