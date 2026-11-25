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
        std::vector<std::string> keys;
        static void Changed(const char*, const char* key, void* context) noexcept
        {
            static_cast<Events*>(context)->keys.emplace_back(key ? key : "*");
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
        static_assert(ParseColor("#FF8000")->rgba == 0xFF8000FFu && ParseColor("#ff800080")->rgba == 0xFF800080u);
        static_assert(!ParseColor("") && !ParseColor("#") && !ParseColor("FF8000") && !ParseColor("#F80") && !ParseColor("#FF80000") &&
            !ParseColor("#FF8000FF0") && !ParseColor("#GG8000") && !ParseColor(" #FF8000") && !ParseColor("#FF8000 ") && !ParseColor("0xFF8000"));
        check(FormatColor(ColorValue{ 0xFF8000FFu }) == "#FF8000" && FormatColor(ColorValue{ 0xFF8000FFu }, true) == "#FF8000FF" &&
            FormatColor(ColorValue{ 0x0A0B0C0Du }) == "#0A0B0C0D" && FormatColor(ColorValue{ 0 }) == "#00000000",
            "colors format as uppercase hex, opaque colors as six digits unless alpha is requested");
        for (const std::uint32_t rgba : { 0u, 0xFFu, 0x12345678u, 0x80808080u, 0xFFFFFF00u, 0xFFFFFFFFu }) {
            check(*ParseColor(FormatColor(ColorValue{ rgba })) == ColorValue{ rgba } && *ParseColor(FormatColor(ColorValue{ rgba }, true)) == ColorValue{ rgba },
                "text form round-trips every channel");
        }

        auto schema = TestJson::parse(R"({
            "schemaVersion":1,"title":"Colors",
            "groups":{"Look":[
                {"key":"tint","type":"color","label":"Tint","default":"#336699"},
                {"key":"glow","type":"color","label":"Glow","default":"#FF800080","alpha":true},
                {"key":"enabled","type":"bool","default":true},
                {"key":"count","type":"int","default":3}
            ]}
        })");
        std::string error;
        const auto parsed = SettingsJson::ParseSchema(schema, "colors", error);
        check(parsed && error.empty(), "color schema parses");
        const auto& tint = std::get<ColorDefinition>(parsed->FindSetting("tint")->definition);
        const auto& glow = std::get<ColorDefinition>(parsed->FindSetting("glow")->definition);
        check(tint.defaultValue.rgba == 0x336699FFu && !tint.alpha && glow.defaultValue.rgba == 0xFF800080u && glow.alpha,
            "six-digit defaults are opaque and alpha defaults to false");

        const auto parses = [&](const TestJson& setting) {
            auto changed = schema;
            changed["groups"]["Look"][0] = setting;
            return SettingsJson::ParseSchema(changed, "colors", error).has_value();
        };
        check(parses({{"key", "tint"}, {"type", "color"}, {"default", "#336699FF"}}), "an opaque eight-digit default is valid without alpha");
        check(parses({{"key", "tint"}, {"type", "color"}, {"default", "#336699"}, {"alpha", true}}), "a six-digit default is valid with alpha");
        check(parses({{"key", "tint"}, {"type", "color"}, {"default", "#abcdef"}}), "lowercase hex is accepted");
        check(!parses({{"key", "tint"}, {"type", "color"}, {"default", "#33669980"}}), "a translucent default needs alpha");
        check(!parses({{"key", "tint"}, {"type", "color"}, {"default", "#33669980"}, {"alpha", false}}), "explicit alpha false rejects translucency");
        check(!parses({{"key", "tint"}, {"type", "color"}}), "color default is required");
        for (const TestJson& value : { TestJson("336699"), TestJson("#369"), TestJson("#3366"), TestJson("#GG6699"), TestJson("#336699 "),
                 TestJson(0x336699), TestJson(true), TestJson(nullptr), TestJson::array({ 0x33, 0x66, 0x99 }) }) {
            check(!parses({{"key", "tint"}, {"type", "color"}, {"default", value}}), "malformed color defaults are schema errors");
        }
        for (const TestJson& alpha : { TestJson(1), TestJson("true"), TestJson(nullptr) }) {
            check(!parses({{"key", "tint"}, {"type", "color"}, {"default", "#336699"}, {"alpha", alpha}}), "alpha must be a boolean");
        }

        const auto& tintSetting = *parsed->FindSetting("tint");
        const auto& glowSetting = *parsed->FindSetting("glow");
        check(IsValidValue(tintSetting, ColorValue{ 0x000000FFu }) && !IsValidValue(tintSetting, ColorValue{ 0x000000FEu }) &&
            IsValidValue(glowSetting, ColorValue{ 0 }) && !IsValidValue(tintSetting, SettingValue{ std::int64_t{ 0x000000FF } }) &&
            !IsValidValue(*parsed->FindSetting("count"), ColorValue{ 3 }), "validation keeps colors distinct and enforces opacity");
        check(SettingsJson::DecodeValue(TestJson("#102030"), tintSetting) == SettingValue{ ColorValue{ 0x102030FFu } } &&
            !SettingsJson::DecodeValue(TestJson(0x102030FF), tintSetting) && !SettingsJson::DecodeValue(TestJson("#102030"), *parsed->FindSetting("count")),
            "values decode only from color text");
        check(SettingsJson::EncodeValues({{"a", ColorValue{ 0x102030FFu }}, {"b", ColorValue{ 0x10203040u }}}) == nlohmann::json{{"a", "#102030"}, {"b", "#10203040"}},
            "opaque values save as six digits and translucent values as eight");

        const auto root = std::filesystem::temp_directory_path() /
            ("osfsettings-colors-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup
        {
            std::filesystem::path root;
            ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
        } cleanup{ root };
        const auto schemas = root / "schemas", values = root / "values", saved = values / "colors.json";
        std::filesystem::create_directories(schemas);
        { std::ofstream output(schemas / "colors.json"); output << schema.dump(); }

        HotkeyInputState input;
        SettingsService backend;
        API::SettingsApi api(backend, input);
        API::Client client;
        std::uint32_t rgba = 7;
        check(client.Attach(&api) && client.Has(API::kColorVersion), "the service implements the color ABI");
        check(client.GetColor("colors", "tint", &rgba) == Status::NotReady && rgba == 7 &&
            client.SetColor("colors", "tint", 0u) == Status::NotReady, "unready color calls preserve outputs");
        backend.Load(schemas, values);
        backend.Start();
        check(backend.LoadErrors().empty(), "fixture loads without diagnostics");
        Events events;
        API::Subscription token{};
        check(client.Subscribe("colors", &Events::Changed, &events, &token) == Status::Ok, "subscribe before reading");
        backend.DispatchChanges(); events.keys.clear();

        check(client.GetColor("colors", "tint", &rgba) == Status::Ok && rgba == 0x336699FFu, "packed default read");
        API::Rgba channels;
        check(client.GetColor("colors", "glow", &channels) == Status::Ok && channels.r == 0xFF && channels.g == 0x80 && channels.b == 0 && channels.a == 0x80,
            "channel read");
        rgba = 7;
        check(client.GetColor(nullptr, "tint", &rgba) == Status::InvalidArgument && client.GetColor("colors", "tint", static_cast<std::uint32_t*>(nullptr)) == Status::InvalidArgument &&
            client.GetColor("missing", "tint", &rgba) == Status::UnknownMod && client.GetColor("colors", "missing", &rgba) == Status::UnknownSetting && rgba == 7,
            "lookup errors preserve the output");
        std::int64_t integer = 5;
        bool enabled = false;
        check(client.GetColor("colors", "count", &rgba) == Status::TypeMismatch && client.GetColor("colors", "enabled", &rgba) == Status::TypeMismatch && rgba == 7 &&
            client.GetInt("colors", "tint", &integer) == Status::TypeMismatch && integer == 5 && client.GetBool("colors", "tint", &enabled) == Status::TypeMismatch &&
            client.SetColor("colors", "count", 0xFFu) == Status::TypeMismatch && client.SetInt("colors", "tint", 0xFF) == Status::TypeMismatch,
            "colors and integers stay distinct types");
        check(client.SetColor("colors", "tint", 0x336699FFu) == Status::Ok && !backend.HasPendingChanges() && !std::filesystem::exists(saved),
            "unchanged color does not save or notify");
        check(client.SetColor("colors", "tint", 0x11223380u) == Status::InvalidValue && !backend.HasPendingChanges(),
            "an opaque-only setting rejects translucency");
        check(client.SetColor("colors", "tint", API::Rgba{ 0xAB, 0xCD, 0xEF }) == Status::Ok && client.SetColor("colors", "glow", 0x00000000u) == Status::Ok &&
            TestJson::parse(Read(saved))["values"] == TestJson{{"glow", "#00000000"}, {"tint", "#ABCDEF"}}, "writes persist color text");
        backend.DispatchChanges();
        check(events.keys == std::vector<std::string>{"tint", "glow"} || events.keys == std::vector<std::string>{"glow", "tint"}, "writes notify their keys");
        events.keys.clear();
        {
            SettingsStore reloaded; reloaded.LoadAll(schemas, values);
            check(reloaded.LoadErrors().empty() && std::get<ColorValue>(*reloaded.GetValue("colors", "tint")).rgba == 0xABCDEFFFu &&
                std::get<ColorValue>(*reloaded.GetValue("colors", "glow")).rgba == 0, "saved colors reload");
        }
        check(client.Reset("colors", "tint") == Status::Ok && client.GetColor("colors", "tint", &rgba) == Status::Ok && rgba == 0x336699FFu &&
            !TestJson::parse(Read(saved))["values"].contains("tint"), "reset restores the authored default");
        client.Unsubscribe(token);

        for (const TestJson& value : { TestJson("#12"), TestJson(0x112233FF), TestJson("#11223380"), TestJson(true) }) {
            { std::ofstream output(saved); output << TestJson{{"formatVersion", 1}, {"values", {{"tint", value}, {"count", 9}}}}.dump(); }
            SettingsStore reloaded; reloaded.LoadAll(schemas, values);
            check(reloaded.LoadErrors().size() == 1 && std::get<ColorValue>(*reloaded.GetValue("colors", "tint")).rgba == 0x336699FFu &&
                std::get<std::int64_t>(*reloaded.GetValue("colors", "count")) == 9, "invalid saved colors fall back to default without discarding siblings");
        }

        std::uint32_t version{};
        check(OSFSettings_RequestAPI(API::kBaseVersion, &version) && version == API::kColorVersion, "1.0 requests are served the 1.1 contract");
        check(OSFSettings_RequestAPI(API::kColorVersion, &version) && version == API::kColorVersion, "1.1 requests are served");
        check(!OSFSettings_RequestAPI(0x00010002u, &version) && version == 0, "newer minors are refused");
        std::cout << checks << " color checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
