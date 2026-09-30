#include "OSFSettings_Providers.h"
#include "API/SettingsApi.h"
#include "Settings/SettingsService.h"
#include "Input/KeyActions.h"
#include "Input/HotkeyInputState.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>

extern "C" void* OSFSettings_RequestProvidersAPI(std::uint32_t, std::uint32_t*) noexcept;

int main()
{
    using namespace OSFSettings;
    using API::Status;
    int checks{};
    auto check = [&](bool condition) { ++checks; if (!condition) throw std::runtime_error("provider check " + std::to_string(checks)); };
    const auto root = std::filesystem::temp_directory_path() /
        ("osfsettings-providers-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path root; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(root, ec); } } cleanup{root};
    try {
        std::filesystem::create_directories(root / "schemas");
        const char* schema = R"({"groups":{"General":[
            {"key":"enabled","type":"bool","default":true},
            {"key":"binding","type":"key","default":4,"allowMouse":true},
            {"key":"count","type":"int","default":2,"min":0,"max":10},
            {"key":"scale","type":"float","default":0.5},
            {"key":"mode","type":"enum","default":"a","options":{"a":"A","b":"B"}}
        ]}})";
        { std::ofstream out(root / "schemas/static.mod.json"); out << schema; }
        auto& backend = SettingsService::Get();
        backend.Load(root / "schemas", root / "values");
        check(backend.Start());
        API::SettingsApi settings{backend, HotkeyInputState::Get()};
        std::uint32_t version = 1;
        check(!OSFSettings_RequestProvidersAPI(0x20000, &version) && version == 0);
        auto* api = static_cast<API::Providers::IProviders*>(OSFSettings_RequestProvidersAPI(API::Providers::kVersion, &version));
        check(api && version == API::Providers::kVersion);
        struct Persistence { bool accept{true}; unsigned writes{}; std::string values; } persistence;
        const auto save = [](const char*, const char* json, void* context) noexcept {
            auto& saved = *static_cast<Persistence*>(context);
            ++saved.writes;
            if (saved.accept) saved.values = json;
            return saved.accept;
        };
        API::Providers::Registration token{};
        check(api->Register("nul.extra", schema, "{}", save, &persistence, &token) == Status::InvalidValue && !token);
        check(persistence.writes == 0 && !backend.FindMod("provider.mod"));
        check(api->Register("static.mod", schema, "{}", save, &persistence, &token) == Status::AlreadyRegistered && token == 0);
        const auto revision = backend.Revision();
        check(api->Register("provider.mod", schema, R"({"enabled":false,"count":5})", save, &persistence, &token) == Status::Ok && token);
        check(backend.Revision() > revision);
        bool enabled = true;
        check(settings.GetBool("provider.mod", "enabled", &enabled) == Status::Ok && !enabled);
        check(!std::filesystem::exists(root / "values/provider.mod.json"));
        API::Providers::Registration intruder{};
        check(api->Register("provider.mod", schema, "{}", save, &persistence, &intruder) == Status::AlreadyRegistered && !intruder);
        check(settings.SetBool("provider.mod", "enabled", true) == Status::Ok);
        check(nlohmann::json::parse(persistence.values)["count"] == 5 && persistence.writes == 1);
        persistence.accept = false;
        check(settings.SetBool("provider.mod", "enabled", false) == Status::SaveFailed);
        check(settings.GetBool("provider.mod", "enabled", &enabled) == Status::Ok && enabled);
        check(settings.SetInt("provider.mod", "count", 11) == Status::InvalidValue && persistence.writes == 2);
        persistence.accept = true;
        check(api->Register("provider.mod", schema, "{}", save, &persistence, &token) == Status::Ok);
        std::int64_t count{};
        check(settings.GetInt("provider.mod", "count", &count) == Status::Ok && count == 5);
        unsigned fired{};
        API::Subscription keyToken{};
        check(api->SubscribeKey("provider.mod", "binding", [](const char*, const char*, void* context) noexcept { ++*static_cast<unsigned*>(context); }, &fired, &keyToken) == Status::Ok);
        KeyActions::Get().Process(4);
        check(fired == 1);
        const auto block = HotkeyInputState::Get().AcquireBlock();
        KeyActions::Get().Process(4);
        check(fired == 1);
        check(HotkeyInputState::Get().ReleaseBlock(block));
        check(settings.SetKey("provider.mod", "binding", 5) == Status::Ok);
        KeyActions::Get().Process(4);
        KeyActions::Get().Process(5);
        check(fired == 2);
        check(api->UnsubscribeKey(keyToken) == Status::Ok);
        KeyActions::Get().Process(5);
        check(fired == 2);
        struct Self { API::Providers::IProviders* api; API::Subscription token{}; unsigned fired{}; } self{api};
        check(api->SubscribeKey("provider.mod", "binding", [](const char*, const char*, void* context) noexcept {
            auto& self = *static_cast<Self*>(context); ++self.fired; self.api->UnsubscribeKey(self.token);
        }, &self, &self.token) == Status::Ok);
        KeyActions::Get().Process(5);
        KeyActions::Get().Process(5);
        check(self.fired == 1);
        check(api->Unregister(token) == Status::Ok);
        check(settings.GetBool("provider.mod", "enabled", &enabled) == Status::UnknownMod);
        check(api->Unregister(token) == Status::InvalidArgument);
        check(settings.GetBool("static.mod", "enabled", &enabled) == Status::Ok);
        std::cout << checks << " provider checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
