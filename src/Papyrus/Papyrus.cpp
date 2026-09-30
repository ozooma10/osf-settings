#include "Papyrus.h"
#include "Subscriptions.h"
#include "Values.h"
#include "Actions.h"
#include "Issues.h"
#include <charconv>
#include "RE/B/BSScriptUtil.h"
#include "RE/E/Events.h"
#include "REL/THook.h"
#include "REX/FModule.h"
#include "Harness/TestHarness.h"

namespace OSFSettings::Papyrus
{
    namespace
    {
        using VM = RE::BSScript::IVirtualMachine;
        using Object = RE::BSTSmartPointer<RE::BSScript::Object>;
        // Preserve VM string bytes on input, and use the case-sensitive pool on output.
        using String = RE::BSFixedStringCS;
        constexpr std::string_view Script = "OSFSettings";
        constexpr std::ptrdiff_t kBindCall = 0x802;
        std::optional<REL::THook<void(VM**)>> g_bindHook;

        void Dispatch(const Receiver& receiver, Subscriptions::Kind kind, const std::string& mod, const std::string& key);

        Subscriptions& Listeners()
        {
            static auto* listeners = new Subscriptions(SettingsService::Get(), HotkeyInputState::Get(), Dispatch);
            return *listeners;
        }
        Values Access() { return Values(SettingsService::Get()); }
        Issues IssueAccess() { return Issues(API::DiagnosticsApi::Get()); }

        // Queues a string-argument event on a bound instance, or on a global script when the handle is empty.
        bool Call(const Receiver& receiver, const char* function, std::vector<std::string> arguments)
        {
            auto* game = RE::GameVM::GetSingleton();
            auto* vm = game ? game->GetVM() : nullptr;
            if (!vm) return false;
            const auto args = [arguments = std::move(arguments)](RE::BSScrapArray<RE::BSScript::Variable>& out) {
                out.resize(static_cast<std::uint32_t>(arguments.size()));
                for (std::uint32_t i = 0; i < arguments.size(); ++i) out[i] = String(arguments[i]);
                return true;
            };
            const RE::BSFixedString name(function);
            const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> noCallback;
            return receiver.handle ?
                vm->DispatchMethodCall(receiver.handle, RE::BSFixedString(receiver.script), name, args, noCallback, 0) :
                vm->DispatchStaticCall(RE::BSFixedString(receiver.script), name, args, noCallback, 0);
        }

        bool DispatchAction(const Receiver& receiver, ActionService::Invocation invocation, const std::string& mod, const std::string& id)
        {
            return Call(receiver, "OnOSFAction", { mod, id, std::to_string(invocation) });
        }
        Actions& ActionHandlers()
        {
            static auto* handlers = new Actions(ActionService::Get(), DispatchAction);
            return *handlers;
        }

        bool IsScriptName(std::string_view text)
        {
            // Papyrus namespaces use ':'; each component is an identifier.
            bool first = true;
            for (const auto c : text) {
                if (c == ':' && !first) { first = true; continue; }
                const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
                if (!letter && (first || c < '0' || c > '9')) return false;
                first = false;
            }
            return !first;
        }

        std::int32_t GetVersion(VM&, std::uint32_t, std::monostate) { return 10000; }
        bool IsReady(VM&, std::uint32_t, std::monostate)
        {
            return SettingsService::Get().IsReady() && !Listeners().IsSuspended();
        }

        bool GetBool(VM&, std::uint32_t, std::monostate, String mod, String key, bool fallback) { 
            return Access().Read("GetBool", mod.c_str(), key.c_str(), fallback); 
        }
        std::int32_t GetInt(VM&, std::uint32_t, std::monostate, String mod, String key, std::int32_t fallback) { 
            return Access().Read("GetInt", mod.c_str(), key.c_str(), fallback); 
        }
        float GetFloat(VM&, std::uint32_t, std::monostate, String mod, String key, float fallback) { 
            return Access().Read("GetFloat", mod.c_str(), key.c_str(), fallback); 
        }
        String GetEnum(VM&, std::uint32_t, std::monostate, String mod, String key, String fallback) { 
            return String(Access().Read("GetEnum", mod.c_str(), key.c_str(), EnumValue{ fallback.c_str() }).value); 
        }
        String GetString(VM&, std::uint32_t, std::monostate, String mod, String key, String fallback) { 
            return String(Access().Read("GetString", mod.c_str(), key.c_str(), std::string(fallback.c_str()))); 
        }

        bool SetBool(VM&, std::uint32_t, std::monostate, String mod, String key, bool value) { 
            return Access().Write("SetBool", mod.c_str(), key.c_str(), value); 
        }
        bool SetInt(VM&, std::uint32_t, std::monostate, String mod, String key, std::int32_t value) { 
            return Access().Write("SetInt", mod.c_str(), key.c_str(), std::int64_t(value)); 
        }
        bool SetFloat(VM&, std::uint32_t, std::monostate, String mod, String key, float value) { 
            return Access().Write("SetFloat", mod.c_str(), key.c_str(), double(value)); 
        }
        bool SetEnum(VM&, std::uint32_t, std::monostate, String mod, String key, String value) { 
            return Access().Write("SetEnum", mod.c_str(), key.c_str(), EnumValue{ value.c_str() }); 
        }
        bool SetString(VM&, std::uint32_t, std::monostate, String mod, String key, String value) { 
            return Access().Write("SetString", mod.c_str(), key.c_str(), std::string(value.c_str())); 
        }
        bool Reset(VM&, std::uint32_t, std::monostate, String mod, String key) { 
            return Access().Reset(mod.c_str(), key.c_str()); 
        }
        bool ResetMod(VM&, std::uint32_t, std::monostate, String mod) { 
            return Access().ResetMod(mod.c_str()); 
        }

        std::optional<Receiver> Instance(VM& vm, const Object& object)
        {
            if (!object || !object->IsValid() || !object->type) return std::nullopt;
            const auto handle = object->GetHandle();
            const auto& policy = vm.GetObjectHandlePolicy();
            if (!handle || handle == policy.EmptyHandle() || !policy.IsHandleObjectAvailable(handle)) return std::nullopt;
            Object bound;
            if (!vm.FindBoundObject(handle, object->type->name.c_str(), false, bound, true) || bound.get() != object.get()) return std::nullopt;
            return Receiver{ handle, FoldAscii(object->type->name.c_str()) };
        }

        std::optional<Receiver> Global(VM& vm, String script)
        {
            if (!IsScriptName(script.c_str())) return std::nullopt;
            RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
            if (!vm.GetScriptObjectType(RE::BSFixedString(script), type) || !type) return std::nullopt;
            return Receiver{ 0, FoldAscii(script.c_str()) };
        }

        bool RegisterTarget(std::string_view function, std::optional<Receiver> receiver, Subscriptions::Kind kind, String mod, String key = {})
        {
            return Report(function, mod.c_str(), key.c_str(), receiver ?
                Listeners().Register(std::move(*receiver), kind, mod.c_str(), key.c_str()) : SettingsError::InvalidArgument);
        }

        bool RegisterForChanges(VM& vm, std::uint32_t, std::monostate, Object object, String mod) { 
            return RegisterTarget("RegisterForChanges", Instance(vm, object), Subscriptions::Kind::Changes, mod); 
        }
        bool RegisterForChangesStatic(VM& vm, std::uint32_t, std::monostate, String script, String mod) { 
            return RegisterTarget("RegisterForChangesStatic", Global(vm, script), Subscriptions::Kind::Changes, mod); 
        }
        bool RegisterHotkey(VM& vm, std::uint32_t, std::monostate, Object object, String mod, String key) { 
            return RegisterTarget("RegisterHotkey", Instance(vm, object), Subscriptions::Kind::Hotkey, mod, key); 
        }
        bool RegisterHotkeyStatic(VM& vm, std::uint32_t, std::monostate, String script, String mod, String key) { 
            return RegisterTarget("RegisterHotkeyStatic", Global(vm, script), Subscriptions::Kind::Hotkey, mod, key); 
        }

        bool RegisterActionTarget(std::optional<Receiver> receiver, String mod, String id)
        {
            if (!receiver || Listeners().IsSuspended()) return false;
            const auto result = ActionHandlers().Register(std::move(*receiver), mod.c_str(), id.c_str());
            if (result != ActionError::None) REX::WARN("Papyrus RegisterAction({}/{}): status {}", mod.c_str(), id.c_str(), static_cast<int>(result));
            return result == ActionError::None;
        }
        bool RegisterAction(VM& vm, std::uint32_t, std::monostate, Object object, String mod, String id)
        {
            return RegisterActionTarget(Instance(vm, object), mod, id);
        }
        bool RegisterActionStatic(VM& vm, std::uint32_t, std::monostate, String script, String mod, String id)
        {
            return RegisterActionTarget(Global(vm, script), mod, id);
        }
        bool CompleteAction(VM&, std::uint32_t, std::monostate, String token, bool succeeded, String message)
        {
            const std::string_view text(token.c_str());
            ActionService::Invocation invocation{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), invocation);
            return error == std::errc{} && end == text.data() + text.size() && invocation && ActionService::Get().Complete(invocation, succeeded, message.c_str()) == ActionError::None;
        }

        bool ReportIssue(VM&, std::uint32_t, std::monostate, String mod, String id, String title, bool isError, String impact, String nextSteps)
        {
            return IssueAccess().ReportIssue(mod.c_str(), id.c_str(), title.c_str(), isError, impact.c_str(), nextSteps.c_str());
        }
        bool ClearIssue(VM&, std::uint32_t, std::monostate, String mod, String id)
        {
            return IssueAccess().ClearIssue(mod.c_str(), id.c_str());
        }
        bool ClearModIssues(VM&, std::uint32_t, std::monostate, String mod)
        {
            return IssueAccess().ClearModIssues(mod.c_str());
        }

        void Bind(VM& vm)
        {
            vm.BindNativeMethod(Script, "GetVersion", &GetVersion, false, false);
            vm.BindNativeMethod(Script, "IsReady", &IsReady, false, false);
            vm.BindNativeMethod(Script, "GetBool", &GetBool, false, false);
            vm.BindNativeMethod(Script, "GetInt", &GetInt, false, false);
            vm.BindNativeMethod(Script, "GetFloat", &GetFloat, false, false);
            vm.BindNativeMethod(Script, "GetEnum", &GetEnum, false, false);
            vm.BindNativeMethod(Script, "GetString", &GetString, false, false);
            vm.BindNativeMethod(Script, "SetBool", &SetBool, false, false);
            vm.BindNativeMethod(Script, "SetInt", &SetInt, false, false);
            vm.BindNativeMethod(Script, "SetFloat", &SetFloat, false, false);
            vm.BindNativeMethod(Script, "SetEnum", &SetEnum, false, false);
            vm.BindNativeMethod(Script, "SetString", &SetString, false, false);
            vm.BindNativeMethod(Script, "Reset", &Reset, false, false);
            vm.BindNativeMethod(Script, "ResetMod", &ResetMod, false, false);
            vm.BindNativeMethod(Script, "RegisterForChanges", &RegisterForChanges, false, false);
            vm.BindNativeMethod(Script, "RegisterForChangesStatic", &RegisterForChangesStatic, false, false);
            vm.BindNativeMethod(Script, "RegisterHotkey", &RegisterHotkey, false, false);
            vm.BindNativeMethod(Script, "RegisterHotkeyStatic", &RegisterHotkeyStatic, false, false);
            vm.BindNativeMethod(Script, "RegisterAction", &RegisterAction, false, false);
            vm.BindNativeMethod(Script, "RegisterActionStatic", &RegisterActionStatic, false, false);
            vm.BindNativeMethod(Script, "CompleteAction", &CompleteAction, false, false);
            vm.BindNativeMethod(Script, "ReportIssue", &ReportIssue, false, false);
            vm.BindNativeMethod(Script, "ClearIssue", &ClearIssue, false, false);
            vm.BindNativeMethod(Script, "ClearModIssues", &ClearModIssues, false, false);
            REX::INFO("Papyrus: OSFSettings native registration attempted");
            TestHarness::BindPapyrus(vm);
        }

        void BindEverythingToScript(VM** vm)
        {
            (*g_bindHook)(vm);
            Bind(**vm);
        }

        void Dispatch(const Receiver& receiver, Subscriptions::Kind kind, const std::string& mod, const std::string& key)
        {
            const auto* function = kind == Subscriptions::Kind::Changes ? "OnOSFSettingChanged" : "OnOSFHotkey";
            if (!Call(receiver, function, { mod, key })) {
                REX::WARN("Papyrus {}.{}({}/{}): receiver unavailable or VM rejected callback", receiver.script, function, mod, key);
            }
        }

        class SessionEvents final : public RE::BSTEventSink<RE::TESLoadGameEvent>, public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent&, RE::BSTEventSource<RE::TESLoadGameEvent>*) override
            {
                Listeners().Clear();
                ActionHandlers().ClearSession();
                Listeners().Resume();
                return RE::BSEventNotifyControl::kContinue;
            }
            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (event.opening && event.menuName == "MainMenu") {
                    Listeners().Clear();
                    ActionHandlers().ClearSession();
                    Listeners().Resume();
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    bool Install()
    {
        if (g_bindHook) {
            return g_bindHook->GetEnabled();
        }
        g_bindHook.emplace("Papyrus::Bind", RE::ID::GameVM::Ctor, kBindCall, &BindEverythingToScript);
        if ( !g_bindHook->Init() || !g_bindHook->Enable()) {
            g_bindHook.reset();
            return false;
        }
        return true;
    }

    void RegisterSinks()
    {
        static bool installed{};
        if (installed) return;
        static auto* events = new SessionEvents;
        RE::TESLoadGameEvent::GetEventSource()->RegisterSink(events);
        RE::UI::GetSingleton()->RegisterSink<RE::MenuOpenCloseEvent>(events);
        installed = true;
        return;
    }

}
