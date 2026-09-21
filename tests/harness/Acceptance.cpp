#include "Acceptance.h"
#include "TestHarness.h"
#include "../../sdk/OSFSettingsRegistry.h"
#include "../../sdk/OSFSettings_Diagnostics.h"
#include "Settings/SettingsService.h"
#include "RE/B/BSScriptUtil.h"
#include "RE/V/VirtualMachine.h"
#include "RE/E/Events.h"
#include <Windows.h>
#undef ERROR
#include <Xinput.h>
#include <cstring>
#include <mutex>

namespace OSFSettings::TestHarness
{
    namespace
    {
        using Json = nlohmann::json;
        using VM = RE::BSScript::IVirtualMachine;
        using String = RE::BSFixedStringCS;
        constexpr auto Mod = "osfacceptance";
        constexpr auto Instance = "OSFSettingsAcceptanceInstance";
        constexpr auto Static = "OSFSettingsAcceptanceStatic";
        std::mutex mutex;
        Json records = Json::array();
        std::uint64_t sequence{};
        API::HotkeyBlock block{};
        bool nativeRegistered{};

        void Record(std::string source, std::string event, std::string key, std::string value, bool ok)
        {
            std::scoped_lock lock(mutex);
            records.push_back({{"sequence", ++sequence}, {"source", source}, {"event", event},
                {"key", key}, {"value", value}, {"ok", ok}, {"thread", ::GetCurrentThreadId()}});
            if (records.size() > 192) records.erase(records.begin());
        }

        void Probe(VM&, std::uint32_t, std::monostate, String source, String event, String key, String value, bool ok)
        {
            Record(source.c_str(), event.c_str(), key.c_str(), value.c_str(), ok);
        }

        class Events final : public RE::BSTEventSink<RE::SaveLoadEvent>, public RE::BSTEventSink<RE::TESLoadGameEvent>,
            public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::SaveLoadEvent& event, RE::BSTEventSource<RE::SaveLoadEvent>*) override
            {
                Record("engine", "saveLoad", std::to_string(static_cast<unsigned>(event.opType)), std::to_string(static_cast<unsigned>(event.status)), true);
                return RE::BSEventNotifyControl::kContinue;
            }
            RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent&, RE::BSTEventSource<RE::TESLoadGameEvent>*) override
            {
                Record("engine", "loaded", "", "", true);
                return RE::BSEventNotifyControl::kContinue;
            }
            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (event.menuName == "MainMenu") Record("engine", event.opening ? "mainMenuOpened" : "mainMenuClosed", "", "", true);
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        void OnNativeHotkey(const char*, const char* key, void*) noexcept
        {
            try { Record("native", "hotkey", key, "", true); } catch (...) {}
        }

        void Require(API::Status status)
        {
            if (status != API::Status::Ok) throw std::runtime_error("public API status " + std::to_string(static_cast<unsigned>(status)));
        }

        Json Execute(const Json& args)
        {
            API::Client settings;
            if (!settings.Init() || !settings.IsReady()) throw std::runtime_error("settings-not-ready");
            const auto operation = args.at("operation").get<std::string>();
            if (operation == "registry") {
                struct Copy { std::string caption; bool valid{}; } copy;
                Require(settings.ReadRegistry(Mod, +[](const API::RegistryView& registry, void* context) noexcept {
                    try {
                        auto& out = *static_cast<Copy*>(context);
                        if (registry.modCount != 1) return;
                        const auto& mod = registry.mods[0];
                        if (std::string_view(mod.id.data, mod.id.size) != Mod) return;
                        for (std::uint32_t g = 0; g < mod.groupCount; ++g) {
                            for (std::uint32_t s = 0; s < mod.groups[g].settingCount; ++s) {
                                const auto& setting = mod.groups[g].settings[s];
                                if (std::string_view(setting.key.data, setting.key.size) != "caption") continue;
                                if (setting.type != API::SettingType::String) return;
                                out.caption.assign(setting.value.text.data, setting.value.text.size);
                                out.valid = setting.maxLength == 16 && setting.requiresRestart &&
                                    std::string_view(setting.defaultValue.text.data, setting.defaultValue.text.size) == "Initial";
                            }
                        }
                    } catch (...) {}
                }, &copy));
                if (!copy.valid) throw std::runtime_error("registry-value-or-metadata-mismatch");
                Record("native", "registry", "caption", copy.caption, true);
            } else if (operation == "setString") {
                Require(settings.SetString(Mod, "caption", args.at("value").get<std::string>()));
            } else if (operation == "setBool") {
                Require(settings.SetBool(Mod, "enabled", args.at("value").get<bool>()));
            } else if (operation == "reset") {
                Require(settings.ResetMod(Mod));
            } else if (operation == "block") {
                if (block) throw std::runtime_error("test-block-already-held");
                Require(settings.AcquireHotkeyBlock(&block));
            } else if (operation == "unblock") {
                Require(settings.ReleaseHotkeyBlock(block));
                block = 0;
            } else if (operation == "issues" || operation == "updateIssue" || operation == "clearIssues") {
                API::Diagnostics::Client diagnostics;
                if (!diagnostics.Init()) throw std::runtime_error("diagnostics-unavailable");
                if (operation == "clearIssues") {
                    Require(diagnostics.ClearMod(Mod));
                    Require(diagnostics.ClearMod("osfacceptance-no-schema"));
                } else if (operation == "updateIssue") {
                    Require(diagnostics.Report({Mod, "long", API::Diagnostics::Severity::Error, "Updated acceptance issue"}));
                } else {
                    std::string detail;
                    for (int i = 0; i < 20; ++i) detail += "Acceptance paragraph " + std::to_string(i + 1) + ": This text verifies that the entire issue can be read while list selection stays unchanged.\n\n";
                    Require(diagnostics.Report({Mod, "long", API::Diagnostics::Severity::Error, "Acceptance issue with long details", detail.c_str(), "Disable the test fixture after completing acceptance."}));
                    Require(diagnostics.Report({"osfacceptance-no-schema", "warning", API::Diagnostics::Severity::Warning, "A mod without a settings schema can report issues"}));
                }
            } else if (operation == "papyrus") {
                const auto function = args.at("function").get<std::string>();
                if (function != "InitializeSettings" && function != "WriteValues" && function != "ReadValues" && function != "InspectFixture")
                    throw std::runtime_error("unknown-papyrus-test-function");
                auto* game = RE::GameVM::GetSingleton();
                auto* vm = game ? game->GetVM() : nullptr;
                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!vm || !player) throw std::runtime_error("papyrus-player-unavailable");
                const auto handle = vm->GetObjectHandlePolicy().GetHandleForObject(static_cast<std::uint32_t>(player->GetFormType()), player);
                RE::BSTSmartPointer<RE::BSScript::Object> object;
                if (!vm->FindBoundObject(handle, Instance, false, object, true)) {
                    if (!vm->CreateObject(RE::BSFixedString(Instance), object) || !object)
                        throw std::runtime_error("instance-script-creation-failed");
                    auto* internal = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                    if (!internal) throw std::runtime_error("internal-vm-unavailable");
                    internal->BindObject(object, handle, false);
                    RE::BSTSmartPointer<RE::BSScript::Object> bound;
                    if (!vm->FindBoundObject(handle, Instance, false, bound, true) || bound.get() != object.get())
                        throw std::runtime_error("instance-script-binding-failed");
                }
                const auto empty = [](RE::BSScrapArray<RE::BSScript::Variable>& out) { out.clear(); return true; };
                const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
                const bool instance = vm->DispatchMethodCall(handle, RE::BSFixedString(Instance), RE::BSFixedString(function), empty, callback, 0);
                const bool global = vm->DispatchStaticCall(RE::BSFixedString(Static), RE::BSFixedString(function), empty, callback, 0);
                if (!instance || !global) throw std::runtime_error("papyrus-test-submission-refused");
                if (!nativeRegistered) {
                    Require(settings.RegisterHotkey(Mod, "pulse", OnNativeHotkey, nullptr));
                    nativeRegistered = true;
                }
            } else throw std::runtime_error("unknown-acceptance-operation");
            return {{"submitted", true}, {"operation", operation}};
        }
    }

    void BindPapyrus(VM& vm)
    {
        vm.BindNativeMethod("OSFSettingsAcceptanceProbe", "Record", &Probe, false, false);
    }

    void RegisterAcceptanceEvents()
    {
        static auto* events = new Events;
        RE::SaveLoadEvent::GetEventSource()->RegisterSink(events);
        RE::TESLoadGameEvent::GetEventSource()->RegisterSink(events);
        RE::UI::GetSingleton()->RegisterSink<RE::MenuOpenCloseEvent>(events);
    }

    Json AcceptanceSnapshot()
    {
        Json result;
        { std::scoped_lock lock(mutex); result = {{"sequence", sequence}, {"records", records}}; }
        auto values = Json::object();
        for (const auto* key : {"enabled", "count", "volume", "mode", "caption"}) {
            if (const auto value = SettingsService::Get().GetValue(Mod, key)) {
                std::visit([&](const auto& current) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(current)>, EnumValue>) values[key] = current.value;
                    else if constexpr (std::is_same_v<std::decay_t<decltype(current)>, KeyBinding>) values[key] = current.keyCode;
                    else values[key] = current;
                }, *value);
            }
        }
        result["values"] = std::move(values);
        auto controllers = Json::array();
        using GetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
        static const auto module = ::LoadLibraryW(L"XInput1_4.dll");
        static const auto getState = module ? reinterpret_cast<GetState>(::GetProcAddress(module, "XInputGetState")) : nullptr;
        for (DWORD slot = 0; slot < 4; ++slot) {
            XINPUT_STATE state{};
            const auto status = getState ? getState(slot, &state) : ERROR_PROC_NOT_FOUND;
            controllers.push_back({{"slot", slot}, {"status", status}, {"connected", status == ERROR_SUCCESS}});
        }
        result["controllers"] = std::move(controllers);
        return result;
    }

    std::uint32_t AcceptanceCommand(const char* input, char* output, std::uint32_t capacity) noexcept
    {
        if (!input || !output || capacity < 65536) return 0;
        Json result;
        try {
            if (!RE::BSService::TaskQueue::IsQueueEnabled() || RE::BSService::TaskQueue::GetDrainOwnerThreadID() != ::GetCurrentThreadId())
                throw std::runtime_error("acceptance-command-outside-native-drain");
            result = {{"ok", true}, {"result", Execute(Json::parse(input))}};
        } catch (const std::exception& error) { result = {{"ok", false}, {"error", error.what()}}; }
        try {
            const auto text = result.dump();
            if (text.size() + 1 > capacity) return 0;
            std::memcpy(output, text.c_str(), text.size() + 1);
            return static_cast<std::uint32_t>(text.size() + 1);
        } catch (...) { return 0; }
    }
}

extern "C" __declspec(dllexport) std::uint32_t __cdecl OSFSettings_TestCommand(const char* input, char* output, std::uint32_t capacity) noexcept
{
    return OSFSettings::TestHarness::AcceptanceCommand(input, output, capacity);
}
