#include "../FileLock.h"
#include "API/SettingsApi.h"
#include "Input/HotkeyInputState.h"
#include "Input/KeyCapture.h"
#include "Input/KeyNames.h"
#include "Settings/SettingsJson.h"
#include "Settings/SettingsService.h"
#include "SFSE/Impl/PCH.h"
#include "RE/B/BSInputDeviceManager.h"

#include <iostream>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <nlohmann/json.hpp>

namespace
{
    RE::BSInputDeviceManager* inputManager{};

    class TestKeyboard final : public RE::BSKeyboardDevice
    {
    public:
        void Initialize() override {}
        void Process(float) override {}
        void Release() override {}
        void Reset() override {}

        bool GetKeyNameFromCode(std::uint32_t keyCode, RE::BSFixedStringCS& name) const override
        {
            ++displayCalls;
            lastCode = keyCode;
            if (keyCode != displayCode || !hasDisplayName) return false;
            name = displayName.c_str();
            return true;
        }

        std::uint32_t GetKeyCodeFromName(const char* name) const override
        {
            ++calls;
            lastName = name;
            return lastName == expectedName ? result : 0xFFFFFFFF;
        }

        std::string expectedName{ "F4" };
        std::uint32_t result{ 0x73 };
        mutable std::string lastName;
        mutable unsigned calls{};
        std::uint32_t displayCode{ 0x20 };
        std::string displayName{ "Engine Space" };
        bool hasDisplayName{ true };
        mutable std::uint32_t lastCode{};
        mutable unsigned displayCalls{};
    };
}

// Test-only engine stand-ins. The production binary uses the relocated singleton
// and the live device's vtable. These definitions do not implement vanilla lookup.
namespace RE
{
    BSInputDeviceManager* BSInputDeviceManager::GetSingleton() { return inputManager; }
    void BSInputEventSingleUser::PerformInputProcessing(const InputEvent*) {}
    BSInputDevice::~BSInputDevice() = default;
    bool BSInputDevice::GetKeyNameFromCode(std::uint32_t, BSFixedStringCS&) const { return false; }
    bool BSInputDevice::GetMappedKeyCode(std::uint32_t, std::uint32_t&) const { return false; }
    std::uint32_t BSInputDevice::GetKeyCodeFromName(const char*) const { return 0xFFFFFFFF; }
}

int TestKeySettings()
{
    using namespace OSFSettings;
    using API::Status;
    using Json = nlohmann::ordered_json;
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        ++checks;
        if (!passed) throw std::runtime_error(std::string("Key settings: ") + message);
    };
    static_assert(API::kVersion == 0x00010000u);
    static_assert(API::kUnboundKey == KeyBinding::Unbound);
    check(KeyCodeFromName("F4") == 0x73 && !KeyCodeFromName("unknown") && KeyCodeFromName("unbound") == KeyBinding::Unbound,
        "missing input manager uses the fallback table and preserves unknown names");
    check(KeyName(0x73) == "Key 0x73", "missing input manager uses the numeric display fallback");
    // The remaining schema/store checks use the live-device stand-in.
    static RE::BSInputDeviceManager manager{};
    inputManager = &manager;
    check(KeyCodeFromName("f4") == 0x73, "missing keyboard uses case-insensitive fallback lookup");
    check(KeyName(0x73) == "Key 0x73", "missing keyboard uses the numeric display fallback");
    static TestKeyboard keyboard;
    manager.devices[0] = &keyboard;
    check(KeyCodeFromName(std::string_view("F4suffix", 2)) == 0x73 && keyboard.lastName == "F4",
        "lookup calls the native virtual method with a terminated copy of the exact view");
    const auto calls = keyboard.calls;
    check(!KeyCodeFromName("") && !KeyCodeFromName(std::string_view("F4\0suffix", 9)) &&
        KeyCodeFromName("UNBOUND") == KeyBinding::Unbound && keyboard.calls == calls,
        "empty names, embedded NUL, and unbound never reach the device");
    check(!KeyCodeFromName("unknown") && keyboard.lastName == "unknown",
        "native not-found result becomes nullopt");
    keyboard.expectedName = "Engine supplied label";
    keyboard.result = 0xE2;
    check(KeyCodeFromName(keyboard.expectedName) == 0xE2,
        "names and codes come from the device without a custom name allowlist");
    keyboard.expectedName = "f4";
    keyboard.result = 0x73;
    check(KeyCodeFromName("f4") == 0x73 && keyboard.lastName == "f4",
        "case handling is delegated to the native device");
    keyboard.expectedName = "F4";
    check(IsBindableKey(0x20) && IsBindableKey(0x41) && IsBindableKey(0xA3) && IsBindableKey(0xB3),
        "Space, letters, right modifiers, and media keys use native virtual-key identity");
    check(!IsBindableKey(0) && !IsBindableKey(0xFF) && !IsBindableKey(0x100) && !IsBindableKey(0xFFFFFFFF) &&
        !IsBindableKey(0x01) && !IsBindableKey(0x02) && !IsBindableKey(0x04) && !IsBindableKey(0x05) &&
        !IsBindableKey(0x06) && !IsBindableKey(0x1B), "reject invalid codes, mouse buttons, and reserved Escape");
    check(KeyName(0x20) == "Engine Space" && keyboard.lastCode == 0x20,
        "display lookup forwards the virtual-key code without a scan-code conversion");
    keyboard.displayName = "Native \xC3\xA9tiquette";
    check(KeyName(0x20) == keyboard.displayName, "native UTF-8 display text is copied before releasing its fixed string");
    keyboard.displayName.clear();
    check(KeyName(0x20) == "Key 0x20", "empty native display name uses numeric fallback");
    keyboard.hasDisplayName = false;
    check(KeyName(0x20) == "Key 0x20", "failed native display lookup uses numeric fallback");
    const auto displayCalls = keyboard.displayCalls;
    check(KeyName(0xFF) == "UNBOUND" && KeyName(0) == "Key 0x00" && KeyName(0x100) == "Key 0x100" &&
        keyboard.displayCalls == displayCalls, "unbound and invalid codes bypass native display lookup");
    keyboard.hasDisplayName = true;
    keyboard.displayName = "Engine Space";
    check(!KeyName(0xA3).empty() && KeyName(0xFF) == "UNBOUND" && KeyName(0) == "Key 0x00",
        "display names have unbound and numeric fallbacks");

    auto document = Json::parse(R"({"schemaVersion":1,"id":"keys","groups":{"main":[
        {"key":"toggle","type":"key","default":"F4"},
        {"key":"required","type":"key","default":163,"allowUnbound":false},
        {"key":"mode","type":"enum","default":"F4","options":["F4","F5"]}
    ]}})");
    std::string error;
    const auto schema = SettingsJson::ParseSchema(document, "keys", error);
    check(schema && error.empty() && std::get<KeyBinding>(schema->FindSetting("toggle")->DefaultValue()).keyCode == 0x73, "schema stores virtual-key defaults");
    const std::pair<Json, std::uint32_t> defaults[] = {
        { 115, 0x73 }, { "F4", 0x73 }, { "unbound", 0xFF }, { 255, 0xFF },
        { 7, 7 }, { 179, 0xB3 }, { 226, 0xE2 }
    };
    for (const auto& [source, expected] : defaults) {
        auto named = document;
        named["groups"]["main"][0]["default"] = source;
        const auto parsed = SettingsJson::ParseSchema(named, "keys", error);
        check(parsed && std::get<KeyBinding>(parsed->FindSetting("toggle")->DefaultValue()).keyCode == expected,
            "names and numeric defaults resolve to native virtual-key identity");
    }
    for (const Json invalid : { Json(false), Json(-1), Json(0), Json(256), Json(0x1B), Json(1), Json(115.0),
        Json(""), Json("unknown"), Json(std::string("F4\0", 3)),
        Json(nullptr), Json(std::uint64_t{ 0xFFFFFFFFFFFFFFFF }) }) {
        auto bad = document;
        bad["groups"]["main"][0]["default"] = invalid;
        check(!SettingsJson::ParseSchema(bad, "keys", error), "invalid key default rejects schema");
    }
    auto bad = document;
    bad["groups"]["main"][1]["default"] = 255;
    check(!SettingsJson::ParseSchema(bad, "keys", error), "explicit false rejects unbound defaults");
    bad["groups"]["main"][1]["default"] = "UNBOUND";
    check(!SettingsJson::ParseSchema(bad, "keys", error), "explicit false also rejects named unbound defaults");
    bad["groups"]["main"][1]["allowUnbound"] = true;
    check(SettingsJson::ParseSchema(bad, "keys", error).has_value(), "unbound default permitted explicitly");
    bad["groups"]["main"][1]["allowUnbound"] = "true";
    check(!SettingsJson::ParseSchema(bad, "keys", error), "allowUnbound must be boolean");

    const auto root = std::filesystem::temp_directory_path() /
        ("osfsettings-keys-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path root; ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); } } cleanup{ root };
    const auto schemas = root / "schemas";
    const auto values = root / "values";
    std::filesystem::create_directories(schemas);
    { std::ofstream file(schemas / "keys.json"); file << document; }
    SettingsService backend;
    HotkeyInputState hotkeys;
    API::SettingsApi api{ backend, hotkeys };
    API::Client client;
    std::uint32_t keyCode = 999;
    check(client.GetKey("keys", "toggle", &keyCode) == Status::NotReady && keyCode == 999, "detached SDK preserves output");
    client.Attach(&api);
    check(client.GetKey("keys", "toggle", &keyCode) == Status::NotReady && keyCode == 999, "key reads wait for readiness");
    backend.Load(schemas, values);
    backend.Start();
    std::vector<std::string> changes;
    SettingsService::Subscription token{};
    backend.Subscribe("keys", [&](const SettingsService::Change& change) { changes.push_back(change.key ? *change.key : "*"); }, token);
    backend.DispatchChanges();
    check(changes == std::vector<std::string>{ "*" }, "initial refresh includes key settings");
    changes.clear();
    check(client.GetKey("keys", "toggle", &keyCode) == Status::Ok && keyCode == 0x73, "SDK reads virtual-key defaults");
    keyCode = 999;
    check(api.GetKey(nullptr, "toggle", &keyCode) == Status::InvalidArgument &&
        api.GetKey("keys", nullptr, &keyCode) == Status::InvalidArgument &&
        api.GetKey("keys", "toggle", nullptr) == Status::InvalidArgument && keyCode == 999,
        "key read argument validation preserves output");
    char buffer[] = "unchanged";
    std::uint32_t required = 99;
    check(api.GetKey("keys", "mode", &keyCode) == Status::TypeMismatch && keyCode == 999 &&
        api.GetEnum("keys", "toggle", buffer, sizeof(buffer), &required) == Status::TypeMismatch &&
        api.GetInt("keys", "toggle", nullptr) == Status::InvalidArgument, "keys and enums are distinct on reads");
    std::int64_t integer = 999;
    check(api.GetInt("keys", "toggle", &integer) == Status::TypeMismatch && integer == 999,
        "numeric key identity remains distinct from integer settings");
    check(api.SetEnum("keys", "toggle", "F5") == Status::TypeMismatch && api.SetKey("keys", "mode", 0x74) == Status::TypeMismatch &&
        api.SetInt("keys", "toggle", 0x74) == Status::TypeMismatch, "key writes require the exact setting type");
    check(api.SetKey(nullptr, "toggle", 0x73) == Status::InvalidArgument &&
        api.SetKey("keys", "toggle", 0x1B) == Status::InvalidValue &&
        api.SetKey("keys", "toggle", 0xFFFFFFFF) == Status::InvalidValue &&
        api.SetKey("keys", "required", API::kUnboundKey) == Status::InvalidValue, "invalid key writes rejected");
    check(client.SetKey("keys", "toggle", 0x73) == Status::Ok && !backend.HasPendingChanges() &&
        !std::filesystem::exists(values / "keys.json"), "unchanged code neither writes nor notifies");
    check(client.SetKey("keys", "toggle", 0x7B) == Status::Ok, "key write succeeds");
    backend.DispatchChanges();
    check(changes == std::vector<std::string>{ "toggle" }, "successful key change notifies");
    { std::ifstream file(values / "keys.json"); Json saved; file >> saved;
        check(saved["formatVersion"] == 1 && saved["values"]["toggle"] == 0x7B, "values save numeric virtual-key identity in format 1"); }
    SettingsStore reloaded;
    reloaded.LoadAll(schemas, values);
    check(std::get<KeyBinding>(*reloaded.GetValue("keys", "toggle")).keyCode == 0x7B, "key persists across reload");
    check(client.SetKey("keys", "toggle", API::kUnboundKey) == Status::Ok && client.GetKey("keys", "toggle", &keyCode) == Status::Ok && keyCode == API::kUnboundKey, "SDK supports explicit unbound identity");
    check(api.Reset("keys", "toggle") == Status::Ok && client.GetKey("keys", "toggle", &keyCode) == Status::Ok && keyCode == 0x73, "reset restores key default");
    api.SetKey("keys", "required", 0x09);
    check(api.ResetMod("keys") == Status::Ok && client.GetKey("keys", "required", &keyCode) == Status::Ok && keyCode == 0xA3, "reset mod includes key defaults");
    backend.DispatchChanges(); changes.clear();
    OSFSettings::Test::FileLock writeLock(values / "keys.json");
    check(api.SetKey("keys", "toggle", 0x75) == Status::SaveFailed && client.GetKey("keys", "toggle", &keyCode) == Status::Ok && keyCode == 0x73 &&
        !backend.HasPendingChanges(), "failed save preserves live binding and emits no notification");
    writeLock.Release();
    { std::ofstream file(values / "keys.json"); file << R"({"formatVersion":1,"values":{"toggle":118,"required":"F5"}})"; }
    reloaded.LoadAll(schemas, values);
    check(std::get<KeyBinding>(*reloaded.GetValue("keys", "toggle")).keyCode == 0x76 &&
        std::get<KeyBinding>(*reloaded.GetValue("keys", "required")).keyCode == 0xA3 && reloaded.LoadErrors().size() == 1,
        "name shortcuts apply only to schema defaults; saved values remain numeric");
    backend.Unsubscribe(token);

    KeyCapture capture;
    using State = KeyCapture::State;
    check(!capture.ShouldConsumeKey(0x73) && !capture.HandleKeyEvent(0x73, true, false), "idle capture lets normal input through");
    capture.BeginCapture();
    capture.HandleKeyEvent(0x0D, true, true); // Opening Accept is still held.
    capture.HandleKeyEvent(0x0D, false, false);
    check(capture.GetSnapshot().state == State::WaitingForKey, "opening key repeat and release cannot choose a binding");
    capture.HandleKeyEvent(0xFF, true, false); capture.HandleKeyEvent(0xFF, false, false);
    check(capture.GetSnapshot().state == State::WaitingForKey, "unbound sentinel cannot be captured");
    capture.HandleKeyEvent(0x0D, true, false);
    check(capture.GetSnapshot().state == State::KeySelected && capture.GetSnapshot().selectedKeyCode == 0x0D && !capture.GetSnapshot().selectedKeyReleased, "Enter can be captured");
    capture.HandleKeyEvent(0x0D, true, true);
    check(capture.GetSnapshot().state == State::KeySelected, "held Enter cannot confirm itself");
    capture.HandleKeyEvent(0x0D, false, false);
    capture.HandleKeyEvent(0x0D, true, false);
    check(capture.GetSnapshot().state == State::ConfirmationRequested && capture.GetSnapshot().selectedKeyReleased, "fresh Enter confirms after release");
    capture.RetryConfirmation();
    check(capture.GetSnapshot().state == State::KeySelected && capture.GetSnapshot().selectedKeyCode == 0x0D, "failed save can retry its candidate");
    capture.EndCapture();
    check(capture.ShouldConsumeKey(0x0D) && capture.HandleKeyEvent(0x0D, true, true) && capture.HandleKeyEvent(0x0D, false, false) && !capture.ShouldConsumeKey(0x0D),
        "confirmation repeat and release do not leak to the menu");
    capture.BeginCapture(); capture.HandleKeyEvent(0x73, true, false); capture.HandleKeyEvent(0x1B, true, false);
    check(capture.GetSnapshot().state == State::Cancelled, "Escape cancels a candidate even while held");
    capture.EndCapture(); capture.HandleKeyEvent(0x73, false, false); capture.HandleKeyEvent(0x1B, false, false);
    capture.BeginCapture(); capture.HandleKeyEvent(0x09, true, false); capture.HandleKeyEvent(0x09, false, false);
    check(capture.GetSnapshot().selectedKeyCode == 0x09 && capture.GetSnapshot().selectedKeyReleased, "menu navigation keys can be assigned");
    capture.RequestCancel();
    check(capture.GetSnapshot().state == State::Cancelled, "external cancellation discards candidate");
    capture.HandleKeyEvent(0x1B, true, false);
    capture.ResetForMenuClose();
    check(!capture.ShouldConsumeKey(0x1B) && capture.GetSnapshot().state == State::Idle, "closed menu drops stale held keys");
    for (const std::uint32_t nativeCode : { 0x20u, 0x44u, 0x41u, 0xA3u, 0xB3u, 0xE2u, 0x07u }) {
        capture.BeginCapture();
        capture.HandleKeyEvent(nativeCode, true, false);
        capture.HandleKeyEvent(nativeCode, false, false);
        const auto selected = capture.GetSnapshot();
        check(selected.state == State::KeySelected && selected.selectedKeyCode == nativeCode && selected.selectedKeyReleased,
            "native event identity survives capture without a name allowlist");
        check(backend.SetValue("keys", "toggle", KeyBinding{ selected.selectedKeyCode }) == SettingsError::None &&
            client.GetKey("keys", "toggle", &keyCode) == Status::Ok && keyCode == nativeCode,
            "capture and SDK use the same virtual-key identity");
        reloaded.LoadAll(schemas, values);
        check(std::get<KeyBinding>(*reloaded.GetValue("keys", "toggle")).keyCode == nativeCode,
            "captured virtual-key identity survives persistence");
        capture.EndCapture();
    }
    capture.BeginCapture();
    for (const std::uint32_t invalidCode : { 0u, 1u, 0x100u, 0xFFFFFFFFu }) {
        capture.HandleKeyEvent(invalidCode, true, false);
        capture.HandleKeyEvent(invalidCode, false, false);
    }
    check(capture.GetSnapshot().state == State::WaitingForKey, "invalid native codes are rejected without narrowing");
    capture.ResetForMenuClose();
    capture.BeginCapture();
    check(!capture.ShouldConsumeMouse(1), "ordinary keyboard capture permits mouse UI controls");
    capture.ResetForMenuClose();
    capture.BeginCapture(true);
    check(capture.ShouldConsumeMouse(4) && !capture.ShouldConsumeMouse(0), "mouse-enabled capture accepts only physical buttons");
    capture.HandleKeyEvent(4, true, false);
    check(capture.GetSnapshot().selectedKeyCode == 4 && capture.ShouldConsumeMouse(4), "middle-button capture retains its release");
    capture.HandleKeyEvent(4, false, false);
    check(!capture.ShouldConsumeMouse(1) && !capture.ShouldConsumeMouse(4), "selected binding permits mouse confirmation after release");
    check(KeyCodeFromName("MOUSE3") == 4 && KeyCodeFromName("mouse5") == 6, "authored mouse names use VK codes");
    capture.ResetForMenuClose();
    return checks;
}

int main()
{
    try {
        const auto checks = TestKeySettings();
        std::cout << checks << " key checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
