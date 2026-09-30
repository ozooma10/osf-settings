#include <SFSE/SFSE.h>
#include <RE/Starfield.h>
#include <RE/B/BSService.h>
#include "OSFSettings.h"
#include "OSFSettingsRegistry.h"
#include "OSFSettings_Diagnostics.h"
#include "OSFSettings_Launcher.h"
#include "OSFSettings_Providers.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <ShlObj.h>
#undef ERROR

namespace
{
    namespace API = OSFSettings::API;
    using Json = nlohmann::ordered_json;
    constexpr auto Mod = "osfsettings-test";
    constexpr auto Provider = "osfsettings-test-provider";
    API::Client settings;
    API::Providers::Client providers;
    API::Launcher::Client launchers;
    API::Diagnostics::Client diagnostics;
    API::Providers::Registration provider{};
    API::Subscription changes{}, providerChanges{}, observer{}, providerObserver{};
    std::atomic_uint64_t hotkeyCount{}, keyCount{}, providerKeyCount{}, changeCount{}, providerChangeCount{}, saveCount{}, actionCount{}, handoffCount{};
    std::atomic_bool rejectSaves{}, blockActive{};
    std::filesystem::path providerFile;

    void Event(std::string message)
    {
        REX::INFO("{}", message);
    }

    void Require(API::Status status)
    {
        if (status != API::Status::Ok) throw std::runtime_error("API status " + std::to_string(static_cast<unsigned>(status)));
    }

    void Notify(std::string message)
    {
        RE::BSService::TaskQueue::GetSingleton()->AddTask([message = std::move(message)] {
            if (auto* source = RE::ShowHUDMessageEvent::GetEventSource()) {
                source->Notify({ RE::BSFixedString(message), RE::BSFixedString(), false, false });
            }
        });
    }

    template<class F> void Later(std::chrono::seconds delay, F work)
    {
        // This SFSE consumer and its SDK clients live for the game process.
        // Delayed jobs only use thread-safe public APIs; engine work is queued separately.
        std::thread([delay, work = std::move(work)] {
            std::this_thread::sleep_for(delay);
            try { work(); } catch (const std::exception& error) { REX::ERROR("Delayed test: {}", error.what()); }
        }).detach();
    }

    void Complete(std::uint64_t invocation, bool success, const std::string& message)
    {
        const auto status = settings.CompleteAction(invocation, success, message.c_str());
        Event(std::format("Action completion: {} (status {})", message, static_cast<unsigned>(status)));
    }

    std::string Readback()
    {
        bool enabled{}; std::int64_t count{}; double amount{}; std::uint32_t key{};
        std::string mode, text;
        Require(settings.GetBool(Mod, "enabled", &enabled));
        Require(settings.GetInt(Mod, "count", &count));
        Require(settings.GetFloat(Mod, "amount", &amount));
        Require(settings.GetEnum(Mod, "mode", mode));
        Require(settings.GetString(Mod, "text", text));
        Require(settings.GetKey(Mod, "observedKey", &key));
        return std::format("enabled={} count={} amount={:.2f} mode={} text='{}' key={}", enabled, count, amount, mode, text, key);
    }

    void OnChanged(const char* mod, const char* key, void*) noexcept
    {
        try {
            if (std::string_view(mod) == Provider) ++providerChangeCount; else ++changeCount;
            Event(std::format("Value notification: {}/{}", mod, key ? key : "<reset>"));
        } catch (...) {}
    }

    void OnKey(const char* mod, const char* id, void* context) noexcept
    {
        try {
            const bool observed = context != nullptr;
            const auto count = observed ? ++*static_cast<std::atomic_uint64_t*>(context) : ++hotkeyCount;
            const auto message = std::format("Test {}: {}/{} #{}", observed ? "key observer" : "native hotkey", mod, id, count);
            Event(message);
            Notify(message);
        } catch (...) {}
    }

    bool SaveProvider(const char*, const char* values, void*) noexcept
    {
        // Never call a Settings API from this transaction callback.
        try {
            if (rejectSaves.load()) { Event("Provider intentionally rejected save; value must remain unchanged"); return false; }
            std::filesystem::create_directories(providerFile.parent_path());
            auto temporary = providerFile; temporary += L".tmp";
            {
                std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
                stream << values;
                stream.close();
                if (!stream) return false;
            }
            if (!::MoveFileExW(temporary.c_str(), providerFile.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return false;
            ++saveCount;
            Event(std::format("Provider saved #{}: {}", saveCount.load(), values));
            return true;
        } catch (...) { return false; }
    }

    void RegisterProvider(bool expanded)
    {
        Json schema = {
            {"title", "OSF Test - Runtime provider"},
            {"description", "Caller-owned persistence. Test save failure and removal from the main test page."},
            {"groups", {{"Provider values", Json::array({
                {{"key","enabled"},{"type","bool"},{"label","Provider toggle"},{"default",true}},
                {{"key","count"},{"type","int"},{"label","Provider integer"},{"default",2},{"min",0},{"max",10}},
                {{"key","amount"},{"type","float"},{"label","Provider slider"},{"default",0.5},{"min",0.0},{"max",1.0},{"step",0.05}},
                {{"key","mode"},{"type","enum"},{"label","Provider enum"},{"default","one"},{"options",Json::array({"one","two","three"})}},
                {{"key","text"},{"type","string"},{"label","Provider text"},{"default","Persist me"},{"maxLength",64}},
                {{"key","key"},{"type","key"},{"label","Provider observed key"},{"hint","Bind, close Settings and press. Observer stops when provider is removed and resumes on re-registration."},{"default",255},{"allowMouse",true}}
            })}}}
        };
        if (expanded) schema["groups"]["Provider values"].push_back({{"key","extra"},{"type","bool"},{"label","Added by replacement - existing values survive"},{"default",true}});
        Json values = Json::object();
        if (!provider && std::filesystem::exists(providerFile)) {
            std::ifstream input(providerFile);
            input >> values;
        }
        Require(providers.Register(Provider, schema.dump().c_str(), values.dump().c_str(), SaveProvider, nullptr, &provider));
        Event(expanded ? "Provider replaced with an extra field" : "Provider registered");
    }

    void BootstrapPapyrus()
    {
        RE::BSService::TaskQueue::GetSingleton()->AddTask([] {
            auto* game = RE::GameVM::GetSingleton();
            auto* vm = game ? game->GetVM() : nullptr;
            const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            const auto args = [](RE::BSScrapArray<RE::BSScript::Variable>& out) { out.clear(); return true; };
            const bool submitted = vm && vm->DispatchStaticCall(RE::BSFixedString("OSFSettingsTestGlobal"), RE::BSFixedString("Bootstrap"), args, callback, 0);
            Event(std::format("Papyrus bootstrap queued={}; check Papyrus status after closing Settings", submitted));
        });
    }

    void OnAction(std::uint64_t invocation, const char*, const char* name, void*) noexcept
    {
        try {
            ++actionCount;
            const std::string_view id(name);
            Event(std::format("Action invoked: {} #{}", id, actionCount.load()));
            std::string result = "Native action completed.";
            if (id == "failure") { Complete(invocation, false, "EXPECTED TEST FAILURE: no game data was changed."); return; }
            if (id == "delayed") {
                Later(std::chrono::seconds(3), [invocation] { Complete(invocation, true, "Deferred native action completed after 3 seconds."); });
                return;
            }
            if (id == "duplicate") {
                Complete(invocation, true, "First completion accepted; inspect log for rejected duplicate.");
                const auto status = settings.CompleteAction(invocation, true, "This must not replace the first result.");
                Event(std::format("Duplicate completion status={} (must be nonzero)", static_cast<unsigned>(status)));
                return;
            }
            if (id == "readback") result = Readback();
            else if (id == "status") {
                result = std::format("Native hotkeys: {}; static key observer: {}; provider key observer: {}; static changes: {}; provider changes: {}; saves: {}; actions: {}; handoffs: {}. Provider: {}; reject saves: {}; static observer: {}; block: {}.",
                    hotkeyCount.load(), keyCount.load(), providerKeyCount.load(), changeCount.load(), providerChangeCount.load(), saveCount.load(), actionCount.load(), handoffCount.load(),
                    provider != 0, rejectSaves.load(), observer != 0, blockActive.load());
            }
            else if (id == "invalid") {
                const auto before = Readback();
                const bool rejected = settings.SetInt(Mod,"count",999) == API::Status::InvalidValue &&
                    settings.SetEnum(Mod,"mode","missing") == API::Status::InvalidValue &&
                    settings.SetString(Mod,"shortText","This is longer than eight bytes") == API::Status::InvalidValue &&
                    settings.SetBool(Mod,"count",false) == API::Status::TypeMismatch;
                if (!rejected || before != Readback()) throw std::runtime_error("Invalid-write validation failed");
                result = "PASS: out-of-range, bad enum, overlong string and wrong type rejected; values preserved.";
            }
            else if (id == "registry") {
                std::uint32_t groups{};
                Require(settings.ReadRegistry(Mod, +[](const API::RegistryView& view, void* context) noexcept {
                    if (view.modCount == 1) *static_cast<std::uint32_t*>(context) = view.mods[0].groupCount;
                }, &groups));
                if (!groups) throw std::runtime_error("Registry returned no test groups");
                result = std::format("PASS: public registry returned {} groups.", groups);
            }
            else if (id == "reset") { Require(settings.ResetMod(Mod)); result = "Only the static test page was reset. Hotkeys and provider values are separate."; }
            else if (id == "providerRegister") { RegisterProvider(false); result = "Provider registered. Look for OSF Test - Runtime provider."; }
            else if (id == "providerReplace") { RegisterProvider(true); result = "Provider replaced. Extra field added; existing valid values retained."; }
            else if (id == "providerRemove") {
                if (provider) { Require(providers.Unregister(provider)); provider = 0; }
                result = "Provider removed. Register again to reload its own saved file.";
            }
            else if (id == "rejectSaves") { rejectSaves = !rejectSaves.load(); result = rejectSaves ? "Save rejection ON. Provider edits must fail without changing values." : "Save rejection OFF. Provider edits should save again."; }
            else if (id == "observer") {
                if (observer) { Require(providers.UnsubscribeKey(observer)); observer = 0; result = "Key observer OFF."; }
                else { Require(providers.SubscribeKey(Mod,"observedKey",OnKey,&keyCount,&observer)); result = "Key observer ON. Bind Observed key, close Settings and press it."; }
            }
            else if (id == "subscription") {
                if (changes) { Require(settings.Unsubscribe(changes)); changes = 0; result = "Native value subscription OFF."; }
                else { Require(settings.Subscribe(Mod,OnChanged,nullptr,&changes)); result = "Native value subscription ON."; }
            }
            else if (id == "block") {
                if (blockActive.exchange(true)) { Complete(invocation,false,"A timed block is already active."); return; }
                API::HotkeyBlock token{};
                const auto status = settings.AcquireHotkeyBlock(&token);
                if (status != API::Status::Ok) { blockActive = false; Require(status); }
                try {
                    Later(std::chrono::seconds(10), [token] {
                        const auto released = settings.ReleaseHotkeyBlock(token);
                        blockActive = false;
                        Event(std::format("Timed hotkey block released: {}", static_cast<unsigned>(released)));
                        Notify("OSF Test: hotkey block released");
                    });
                } catch (...) { settings.ReleaseHotkeyBlock(token); blockActive = false; throw; }
                result = "All OSF hotkeys/observers blocked for 10 real seconds. Close now and try them; use Pause to reopen Settings.";
            }
            else if (id == "issues") {
                Require(diagnostics.Report({Mod,"warning",API::Diagnostics::Severity::Warning,"EXPECTED TEST: sample warning","Tests warning presentation.","Use Clear test issues when finished."}));
                std::string detail;
                for (int i=1; i<=12; ++i) detail += std::format("Paragraph {}: scroll this long issue detail. No real game problem is being reported.\n\n",i);
                Require(diagnostics.Report({Mod,"error",API::Diagnostics::Severity::Error,"EXPECTED TEST: long error details",detail.c_str(),"Scroll to paragraph 12, then replace or clear the report."}));
                Require(diagnostics.Report({"osfsettings-test-no-page","orphan",API::Diagnostics::Severity::Warning,"EXPECTED TEST: issue from a mod without a settings page"}));
                result = "Three intentional issues reported. Open Mod Issues.";
            }
            else if (id == "replaceIssue") { Require(diagnostics.Report({Mod,"warning",API::Diagnostics::Severity::Error,"EXPECTED TEST: warning replaced with error"})); result = "Same issue ID replaced; old impact/next steps should be gone."; }
            else if (id == "clearIssue") { Require(diagnostics.Clear(Mod,"warning")); result = "Warning/replacement issue cleared; other issues remain."; }
            else if (id == "clearIssues") { Require(diagnostics.ClearMod(Mod)); Require(diagnostics.ClearMod("osfsettings-test-no-page")); result = "Native test issues cleared."; }
            else if (id == "availability") {
                static bool available = false; available = !available;
                Require(launchers.SetAvailable(Mod,"toggle",available,"Intentionally disabled. Use Toggle launcher availability on the test page."));
                result = available ? "Toggle launcher enabled." : "Toggle launcher disabled.";
            }
            else if (id == "papyrusInit") { BootstrapPapyrus(); result = "Bootstrap queued. Close Settings, wait, reopen and inspect Papyrus status. Requires OSFSettingsTestMod.esm enabled."; }
            Complete(invocation,true,result);
        } catch (const std::exception& error) { Complete(invocation,false,error.what()); }
        catch (...) { Complete(invocation,false,"Unexpected test consumer error; inspect its log."); }
    }

    void AfterClose(const char*, const char* id, std::uint64_t request, void*) noexcept
    {
        try {
            ++handoffCount;
            Event(std::format("After-close handoff: {} request={}",id,request));
            RE::BSService::TaskQueue::GetSingleton()->AddTask([] {
                if (auto* queue = RE::UIMessageQueue::GetSingleton()) queue->AddMessage(RE::BSFixedString("InventoryMenu"),RE::UI_MESSAGE_TYPE::kShow);
            });
        } catch (...) {}
    }

    void OnLaunch(const char*, const char* name, std::uint64_t request, void*) noexcept
    {
        try {
            const std::string id(name);
            Event(std::format("Launcher requested: {} token={}",id,request));
            if (id == "reject") { Require(launchers.Complete(request,nullptr,nullptr,"EXPECTED TEST: destination refused. Settings should stay open.")); return; }
            if (id == "timeout") { Event("Intentionally withholding completion; expect Settings timeout after 30 seconds"); return; }
            if (id == "delay" || id == "late") {
                Later(std::chrono::seconds(id == "late" ? 35 : 5), [request] {
                    const auto status = launchers.Complete(request,AfterClose);
                    Event(std::format("Delayed launcher completion status={} (nonzero expected after cancel/timeout)",static_cast<unsigned>(status)));
                });
                return;
            }
            Require(launchers.Complete(request,AfterClose));
        } catch (const std::exception& error) { REX::ERROR("Launcher test: {}",error.what()); }
    }

    void Initialize()
    {
        if (!settings.Init() || !settings.IsReady() || !providers.Init() || !launchers.Init() || !diagnostics.Init())
            throw std::runtime_error("Matching production OSF Settings APIs unavailable");
        PWSTR documents{};
        const auto found = ::SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&documents);
        if (FAILED(found)) throw std::runtime_error("Documents folder unavailable");
        providerFile = std::filesystem::path(documents) / L"My Games/Starfield/OSF/SettingsTestMod/provider.json";
        ::CoTaskMemFree(documents);
        Require(settings.Subscribe(Mod,OnChanged,nullptr,&changes));
        Require(settings.Subscribe(Provider,OnChanged,nullptr,&providerChanges));
        Require(settings.RegisterHotkey(Mod,"native",OnKey,nullptr));
        Require(settings.RegisterHotkey(Mod,"nativeUnbound",OnKey,nullptr));
        Require(providers.SubscribeKey(Mod,"observedKey",OnKey,&keyCount,&observer));
        Require(providers.SubscribeKey(Provider,"key",OnKey,&providerKeyCount,&providerObserver));
        for (const auto* action : {"success","confirmed","failure","delayed","duplicate","readback","status","invalid","registry","reset",
                "providerRegister","providerReplace","providerRemove","rejectSaves","observer","subscription","block","issues","replaceIssue","clearIssue","clearIssues","availability","papyrusInit"})
            Require(settings.RegisterAction(Mod,action,OnAction,nullptr));
        for (const auto& [id,title] : {std::pair{"success","TEST: callback -> Inventory"}, {"delay","TEST: 5-second handoff / cancel"},
                {"reject","TEST: rejected launcher"},{"timeout","TEST: 30-second timeout"},{"late","TEST: completion after timeout"},{"toggle","TEST: toggle availability"}}) {
            Require(launchers.Register({.modId=Mod,.id=id,.modTitle="OSF Settings Test Mod",.title=title,
                .description="Intentional launcher acceptance case. Read the test mod checklist for expected behavior.",.open=OnLaunch}));
        }
        Require(launchers.SetAvailable(Mod,"toggle",false,"Intentionally disabled. Enable from the test page."));
        RegisterProvider(false);
        Event("Test mod ready. Open OSF Settings Test Mod; use Show test counters and Read all values.");
    }

    void OnMessage(SFSE::MessagingInterface::Message* message)
    {
        if (!message || message->type != SFSE::MessagingInterface::kPostLoad) return;
        try { Initialize(); }
        catch (const std::exception& error) { REX::ERROR("Test mod initialization failed: {}",error.what()); }
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* sfse)
{
    SFSE::Init(sfse,{.logRotate=1,.hook=false});
    const auto* messaging = SFSE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(OnMessage);
}
