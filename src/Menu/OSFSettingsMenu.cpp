#include "OSFSettingsMenu.h"
#include "Harness/TestHarness.h"
#include "FloatSlider.h"
#include "Input/KeyNames.h"
#include "Input/NativeHotkeys.h"
#include <cmath>
#include "Core/Runtime.h"
#include "Diagnostics/DiagnosticsService.h"
#include "Actions/ActionService.h"
#include "Settings/Localization.h"
#include <charconv>
#include <chrono>
#include <Windows.h>
#include <shellapi.h>
#undef ERROR
#include "RE/U/UI.h"
#include "RE/U/UIMessageQueue.h"
#include "RE/B/BSService.h"

namespace OSFSettings
{
    namespace
    {
        enum class Function : std::uintptr_t { GetRows = 1, SetBool, SetInt, SetFloat, SetEnum, Close, Startup, StartupFailed,
            SetKey, BeginKeyCapture, PollKeyCapture, CommitKeyCapture, CancelKeyCapture, BeginNativeBinding, EndNativeBinding, GetIssues,
            RequestBindings, PollBindings, TextInput, SetString, InvokeAction, Revision, Launch, GetLocalization, PollLaunch, OpenIssueModPage };
        // Bound the loading card's wait; expiry invalidates the request.
        constexpr auto kOpenTimeout = std::chrono::seconds(30);
        std::string ArgString(const RE::Scaleform::GFx::FunctionHandler::Params& params, std::uint32_t index)
        {
            return index < params.argCount && params.args[index].IsString() ? params.args[index].GetString() : "";
        }

        std::optional<std::int64_t> ParseInteger(std::string_view text)
        {
            std::int64_t value{};
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return error == std::errc{} && end == text.data() + text.size() ? std::optional(value) : std::nullopt;
        }

        void Text(RE::Scaleform::GFx::Value& object, const char* name, const std::string& value)
        {
            object.SetMember(name, RE::Scaleform::GFx::Value(value.c_str()));
        }

        using Context = RE::ControlMap::InputContextID;
        // Text editing temporarily replaces these contexts.
        constexpr Context kNavigationContexts[]{ Context::kBasicMenuNav, Context::kLeftThumbstick, Context::kVirtualController };

        // Callback destinations open in place; native destinations need their menu registered.
        bool MenuRegistered(const LaunchDestination& destination)
        {
            if (destination.menu.empty()) return true;
            const auto* ui = RE::UI::GetSingleton();
            return ui && ui->IsMenuRegistered(RE::BSFixedString(destination.menu.c_str()));
        }
    }

    OSFSettingsMenu::OSFSettingsMenu()
    {
        menuName = MENU_NAME.data();
        // Native menu-mode lifecycle suppresses console-command hotkeys.
        SetFlags(RE::IMenu::kUsesMenuMode | RE::IMenu::kPausesGame | RE::IMenu::kBlocksLowerMenuInput | RE::IMenu::ShowCursor | RE::IMenu::kModal);
    }

    void OSFSettingsMenu::PostCreate()
    {
        // Dynamic context updates apply only below priority 0x0C. Match PauseMenu; the newly opened menu is stacked above it.
        depthPriority = 0x0B;
        for (const auto context : kNavigationContexts) {
            AddInputContext(context);
        }
    }

    OSFSettingsMenu::~OSFSettingsMenu()
    {
        ++*m_textRequests;
    }

    bool OSFSettingsMenu::RequestTextInput(bool enabled)
    {
        const auto generation = ++*m_textRequests;
        RE::BSService::TaskQueue::GetSingleton()->AddTask([weak = std::weak_ptr(m_textRequests), generation, enabled] {
            const auto request = weak.lock();
            if (!request || request->load() != generation) return;
            auto* ui = RE::UI::GetSingleton();
            if (!ui) return;
            const RE::BSFixedString name(MENU_NAME.data());
            auto current = ui->GetMenu(name);
            if (!current || !ui->IsMenuOpen(name)) return;
            auto* menu = static_cast<OSFSettingsMenu*>(current.get());
            if (menu->m_textRequests != request || menu->m_textInputActive == enabled) return;
            // Text editing must not inherit Accept/E or navigation/WASD.
            menu->inputContexts.end = menu->inputContexts.begin;
            if (enabled) {
                menu->AddInputContext(InputContextID::kTextInput);
            } else {
                for (const auto context : kNavigationContexts) {
                    menu->AddInputContext(context);
                }
            }
            menu->m_textInputActive = enabled;
        });
        return true;
    }

    void OSFSettingsMenu::MapCodeObjectFunctions()
    {
        RegisterNativeFunction("getLocalization", static_cast<std::uint64_t>(Function::GetLocalization));
        RegisterNativeFunction("getRows", static_cast<std::uint64_t>(Function::GetRows));
        RegisterNativeFunction("getIssues", static_cast<std::uint64_t>(Function::GetIssues));
        RegisterNativeFunction("openIssueModPage", static_cast<std::uint64_t>(Function::OpenIssueModPage));
        RegisterNativeFunction("invokeAction", static_cast<std::uint64_t>(Function::InvokeAction));
        RegisterNativeFunction("revision", static_cast<std::uint64_t>(Function::Revision));
        RegisterNativeFunction("launch", static_cast<std::uint64_t>(Function::Launch));
        RegisterNativeFunction("pollLaunch", static_cast<std::uint64_t>(Function::PollLaunch));
        RegisterNativeFunction("requestBindings", static_cast<std::uint64_t>(Function::RequestBindings));
        RegisterNativeFunction("pollBindings", static_cast<std::uint64_t>(Function::PollBindings));
        RegisterNativeFunction("textInput", static_cast<std::uint64_t>(Function::TextInput));
        RegisterNativeFunction("setBool", static_cast<std::uint64_t>(Function::SetBool));
        RegisterNativeFunction("setInt", static_cast<std::uint64_t>(Function::SetInt));
        RegisterNativeFunction("setFloat", static_cast<std::uint64_t>(Function::SetFloat));
        RegisterNativeFunction("setEnum", static_cast<std::uint64_t>(Function::SetEnum));
        RegisterNativeFunction("setString", static_cast<std::uint64_t>(Function::SetString));
        RegisterNativeFunction("setKey", static_cast<std::uint64_t>(Function::SetKey));
        RegisterNativeFunction("beginKeyCapture", static_cast<std::uint64_t>(Function::BeginKeyCapture));
        RegisterNativeFunction("pollKeyCapture", static_cast<std::uint64_t>(Function::PollKeyCapture));
        RegisterNativeFunction("commitKeyCapture", static_cast<std::uint64_t>(Function::CommitKeyCapture));
        RegisterNativeFunction("cancelKeyCapture", static_cast<std::uint64_t>(Function::CancelKeyCapture));
        RegisterNativeFunction("beginNativeBinding", static_cast<std::uint64_t>(Function::BeginNativeBinding));
        RegisterNativeFunction("endNativeBinding", static_cast<std::uint64_t>(Function::EndNativeBinding));
        RegisterNativeFunction("close", static_cast<std::uint64_t>(Function::Close));
        RegisterNativeFunction("startup", static_cast<std::uint64_t>(Function::Startup));
        RegisterNativeFunction("startupFailed", static_cast<std::uint64_t>(Function::StartupFailed));
        TestHarness::RegisterMenuFunctions(*this);
    }

    void OSFSettingsMenu::OnStartupFailed(std::string_view message)
    {
        REX::ERROR("OSF Settings menu startup failed: {}", message);
        Close();
    }

    void OSFSettingsMenu::Call(const RE::Scaleform::GFx::FunctionHandler::Params& params) noexcept
    {
        if (!params.ret || !params.movie || !params.movie->asMovieRoot) return;
        if (TestHarness::HandleMenuCall(params)) return;
        
        auto* root = params.movie->asMovieRoot.get();
        const auto function = static_cast<Function>(reinterpret_cast<std::uintptr_t>(params.userData));
        auto& runtime = Runtime::Get();
        switch (function) {
        case Function::Launch: {
            // 0 = rejected, 1 = closing (handoff queued), 2 = loading (pollLaunch reports the outcome).
            double result = 0;
            auto* ui = RE::UI::GetSingleton();
            if (!m_launch && ui && params.argCount == 2 && params.args[0].IsString() && params.args[1].IsString()) {
                auto destination = LauncherService::Get().Find(ArgString(params, 0), ArgString(params, 1));
                if (destination && destination->available && MenuRegistered(*destination)) {
                    if (destination->open) {
                        const auto request = LauncherService::Get().BeginOpen(destination->mod, destination->id);
                        if (request) {
                            m_launch = PendingLaunch{ std::move(*destination), request, std::chrono::steady_clock::now() + kOpenTimeout };
                            const auto& target = m_launch->destination;
                            REX::INFO("LaunchTrace: open requested request={} view='{}/{}'", request, target.mod, target.id);
                            target.open(target.mod, target.id, request);
                            result = 2;
                        }
                    } else {
                        m_launch = PendingLaunch{ std::move(*destination) };
                        CommitLaunch();
                        result = 1;
                    }
                }
            }
            *params.ret = RE::Scaleform::GFx::Value(result);
            break;
        }
        case Function::PollLaunch: {
            root->CreateObject(params.ret);
            const bool waiting = m_launch && !m_launch->closing;
            std::string state = waiting ? "pending" : m_launch ? "closing" : "idle", message;
            auto report = waiting ? LauncherService::Get().TakeResult(m_launch->requestId) : std::nullopt;
            if (report) {
                REX::INFO("LaunchTrace: open result request={} ready={} reason='{}'", m_launch->requestId, bool(report->afterClose), report->reason);
                if (report->afterClose) {
                    m_launch->afterClose = std::move(report->afterClose);
                    CommitLaunch();
                    state = "closing";
                } else {
                    state = "failed";
                    message = report->reason;
                    AbandonLaunch();
                }
            } else if (waiting && std::chrono::steady_clock::now() >= m_launch->deadline) {
                REX::INFO("LaunchTrace: open timed out request={}", m_launch->requestId);
                AbandonLaunch();
                state = "failed";
            }
            Text(*params.ret, "state", state);
            Text(*params.ret, "message", message);
            break;
        }
        case Function::GetLocalization: {
            root->CreateObject(params.ret);
            const auto catalog = Localization::Get();
            for (const auto& [key, value] : catalog->UI()) {
                Text(*params.ret, key.c_str(), value);
            }
            break;
        }
        case Function::Revision:
            root->CreateString(params.ret, (std::to_string(SettingsService::Get().Revision()) + ":" + std::to_string(ActionService::Get().Revision()) + ":" +
                std::to_string(LauncherService::Get().Revision()) + ":" +
                std::to_string(DiagnosticsService::Get().Revision())).c_str());
            break;
        case Function::InvokeAction: {
            ActionService::Invocation invocation{};
            auto result = ActionError::InvalidArgument;
            std::string error;
            if (params.argCount == 2 && params.args[0].IsString() && params.args[1].IsString()) {
                result = ActionService::Get().Invoke(ArgString(params, 0), ArgString(params, 1), invocation);
            }
            if (result != ActionError::None) {
                error = result == ActionError::Busy ? tr("actions.busy") : tr("actions.unavailable");
            }
            root->CreateObject(params.ret);
            params.ret->SetMember("ok", RE::Scaleform::GFx::Value(result == ActionError::None));
            Text(*params.ret, "error", error);
            break;
        }
        case Function::TextInput:
            *params.ret = RE::Scaleform::GFx::Value(RequestTextInput(params.argCount && params.args[0].IsBoolean() && params.args[0].GetBoolean()));
            break;
        case Function::RequestBindings:
            *params.ret = RE::Scaleform::GFx::Value(RequestBindingSnapshot(m_bindings));
            break;
        case Function::PollBindings: {
            const auto snapshot = m_bindings->Read();
            root->CreateObject(params.ret);
            params.ret->SetMember("generation", RE::Scaleform::GFx::Value(snapshot.generation));
            Text(*params.ret, "state", snapshot.status == BindingSnapshot::Status::Ready ? "ready" :
                snapshot.status == BindingSnapshot::Status::Loading ? "loading" : "unavailable");
            RE::Scaleform::GFx::Value records;
            root->CreateArray(&records);
            for (const auto& entry : snapshot.records) {
                RE::Scaleform::GFx::Value record;
                root->CreateObject(&record);
                Text(record, "action", entry.action);
                record.SetMember("context", RE::Scaleform::GFx::Value(entry.context));
                record.SetMember("device", RE::Scaleform::GFx::Value(entry.device));
                record.SetMember("slot", RE::Scaleform::GFx::Value(entry.slot));
                record.SetMember("key", RE::Scaleform::GFx::Value(entry.key));
                record.SetMember("modifier", RE::Scaleform::GFx::Value(entry.modifier));
                records.PushBack(record);
            }
            params.ret->SetMember("records", records);
            break;
        }
        case Function::Close:
            Close();
            break;
        case Function::Startup:
            REX::INFO("OSF Settings menu movie: {}", ArgString(params, 0));
            break;
        case Function::StartupFailed:
            OnStartupFailed(ArgString(params, 0));
            break;
        case Function::BeginKeyCapture: {
            const auto mod = ArgString(params, 0);
            const auto key = ArgString(params, 1);
            const auto& settings = SettingsService::Get();
            const auto record = settings.IsReady() ? settings.FindMod(mod) : std::nullopt;
            const auto* setting = record ? record->schema.FindSetting(key) : nullptr;
            const auto* binding = setting ? std::get_if<KeyDefinition>(&setting->definition) : nullptr;
            const bool ok = m_capture.GetSnapshot().state == KeyCapture::State::Idle && binding;
            if (ok) {
                m_captureMod = mod;
                m_captureKey = key;
                m_capture.BeginCapture(binding->allowMouse);
            }
            root->CreateObject(params.ret);
            params.ret->SetMember("ok", RE::Scaleform::GFx::Value(ok));
            break;
        }
        case Function::PollKeyCapture: {
            const auto snapshot = m_capture.GetSnapshot();
            const char* state = "idle";
            switch (snapshot.state) {
            case KeyCapture::State::WaitingForKey: state = "waiting"; break;
            case KeyCapture::State::KeySelected: state = "candidate"; break;
            case KeyCapture::State::ConfirmationRequested: state = "confirmed"; break;
            case KeyCapture::State::Cancelled: state = "cancelled"; break;
            default: break;
            }
            root->CreateObject(params.ret);
            Text(*params.ret, "state", state);
            Text(*params.ret, "name", KeyName(snapshot.selectedKeyCode));
            params.ret->SetMember("released", RE::Scaleform::GFx::Value(snapshot.selectedKeyReleased));
            break;
        }
        case Function::CommitKeyCapture: {
            const auto snapshot = m_capture.GetSnapshot();
            const bool canSaveBinding = snapshot.selectedKeyReleased && (snapshot.state == KeyCapture::State::KeySelected || snapshot.state == KeyCapture::State::ConfirmationRequested);
            const bool ok = canSaveBinding && runtime.SetValue(m_captureMod, m_captureKey, KeyBinding{ snapshot.selectedKeyCode }) == SettingsError::None;
            if (ok) m_capture.EndCapture();
            else m_capture.RetryConfirmation();
            root->CreateObject(params.ret);
            params.ret->SetMember("ok", RE::Scaleform::GFx::Value(ok));
            Text(*params.ret, "error", ok ? "" : tr("errors.save"));
            break;
        }
        case Function::CancelKeyCapture:
            m_capture.EndCapture();
            break;
        case Function::BeginNativeBinding:
            *params.ret = RE::Scaleform::GFx::Value(m_capture.GetSnapshot().state == KeyCapture::State::Idle && m_bindingEditor.Begin());
            break;
        case Function::EndNativeBinding:
            m_bindingEditor.End(params.argCount > 0 && params.args[0].IsBoolean() && params.args[0].GetBoolean());
            break;
        case Function::OpenIssueModPage: {
            const auto mod = ArgString(params, 0), id = ArgString(params, 1);
            bool opened = false;
            for (const auto& issue : DiagnosticsService::Get().Snapshot()) {
                if (issue.modId != mod || issue.id != id || !issue.nexusModId) continue;
                const auto url = std::format(L"https://www.nexusmods.com/starfield/mods/{}", issue.nexusModId);
                opened = reinterpret_cast<std::intptr_t>(ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
                break;
            }
            *params.ret = RE::Scaleform::GFx::Value(opened);
            break;
        }
        case Function::GetIssues: {
            const auto issues = DiagnosticsService::Get().Snapshot();
            const auto settings = runtime.Settings();
            root->CreateArray(params.ret);
            for (const auto& issue : issues) {
                RE::Scaleform::GFx::Value row;
                root->CreateObject(&row);
                Text(row, "type", "issue");
                Text(row, "mod", issue.modId);
                Text(row, "id", issue.id);
                Text(row, "modTitle", IssueModName(issue, settings));
                Text(row, "severity", issue.severity == IssueSeverity::Error ? "ERROR" : "WARNING");
                Text(row, "severityLabel", tr(issue.severity == IssueSeverity::Error ? "issues.error" : "issues.warning"));
                Text(row, "title", issue.title);
                Text(row, "impact", issue.impact);
                Text(row, "nextSteps", issue.nextSteps);
                row.SetMember("nexusModId", RE::Scaleform::GFx::Value(issue.nexusModId));
                params.ret->PushBack(row);
            }
            break;
        }
        case Function::GetRows: {
            root->CreateArray(params.ret);
            // Fields shared by setting, action, and hotkey rows.
            const auto modRow = [root](const ModSettings& mod, const SettingsGroup& group, const std::string& key, const std::string& title) {
                RE::Scaleform::GFx::Value row;
                root->CreateObject(&row);
                Text(row, "mod", mod.schema.id);
                Text(row, "modTitle", mod.schema.title);
                Text(row, "modDescription", mod.schema.description);
                Text(row, "group", group.id);
                Text(row, "groupTitle", group.label);
                Text(row, "key", key);
                Text(row, "title", title);
                return row;
            };
            for (const auto& mod : runtime.Settings()) {
                for (const auto& group : mod.schema.groups) {
                    for (const auto& control : group.controls) {
                        if (const auto* action = std::get_if<ActionDefinition>(&control)) {
                            const auto state = ActionService::Get().Status(mod.schema.id, action->id);
                            auto row = modRow(mod, group, action->id, action->label);
                            Text(row, "type", "action");
                            Text(row, "hint", action->hint);
                            Text(row, "confirmation", action->confirmation);
                            Text(row, "message", state.message);
                            Text(row, "actionState", state.state == ActionState::Running ? tr("actions.working") : !state.available ? tr("actions.stateUnavailable") :
                                state.state == ActionState::Succeeded ? tr("actions.stateCompleted") : state.state == ActionState::Failed ? tr("actions.stateFailed") : tr("actions.stateRun"));
                            row.SetMember("editable", RE::Scaleform::GFx::Value(state.available));
                            params.ret->PushBack(row);
                            continue;
                        }
                        const auto& setting = std::get<SettingDefinition>(control);
                        const auto value = mod.values.find(setting.key);
                        if (value == mod.values.end()) continue;
                        auto row = modRow(mod, group, setting.key, setting.label);
                        Text(row, "hint", setting.hint);
                        row.SetMember("requiresRestart", RE::Scaleform::GFx::Value(setting.requiresRestart));
                        if (const auto* definition = std::get_if<BoolDefinition>(&setting.definition)) {
                            Text(row, "type", "bool");
                            row.SetMember("value", RE::Scaleform::GFx::Value(std::get<bool>(value->second)));
                            row.SetMember("defaultValue", RE::Scaleform::GFx::Value(definition->defaultValue));
                            row.SetMember("editable", RE::Scaleform::GFx::Value(true));
                        } else if (std::holds_alternative<IntDefinition>(setting.definition)) {
                            const auto& integer = std::get<IntDefinition>(setting.definition);
                            Text(row, "type", "int");
                            Text(row, "value", std::to_string(std::get<std::int64_t>(value->second)));
                            Text(row, "defaultValue", std::to_string(integer.defaultValue));
                            if (integer.minimum) Text(row, "minimum", std::to_string(*integer.minimum));
                            if (integer.maximum) Text(row, "maximum", std::to_string(*integer.maximum));
                            // AS3 Number must preserve both bounds; BSSlider stores its range as uint32.
                            const bool editable = integer.minimum && integer.maximum && *integer.minimum < *integer.maximum &&
                                *integer.minimum >= -kMaxSafeInteger && *integer.maximum <= kMaxSafeInteger &&
                                *integer.maximum - *integer.minimum <= 4294967295LL;
                            row.SetMember("editable", RE::Scaleform::GFx::Value(editable));
                        } else if (const auto* floating = std::get_if<FloatDefinition>(&setting.definition)) {
                            Text(row, "type", "float");
                            row.SetMember("value", RE::Scaleform::GFx::Value(std::get<double>(value->second)));
                            row.SetMember("defaultValue", RE::Scaleform::GFx::Value(floating->defaultValue));
                            const auto slider = MakeFloatSlider(*floating);
                            row.SetMember("editable", RE::Scaleform::GFx::Value(slider.has_value()));
                            row.SetMember("decimals", RE::Scaleform::GFx::Value(slider ? slider->decimals : -1));
                            if (slider) {
                                row.SetMember("sliderMinimum", RE::Scaleform::GFx::Value(static_cast<double>(slider->minimum)));
                                row.SetMember("sliderMaximum", RE::Scaleform::GFx::Value(static_cast<double>(slider->maximum)));
                                row.SetMember("sliderStep", RE::Scaleform::GFx::Value(static_cast<double>(slider->step)));
                                row.SetMember("sliderScale", RE::Scaleform::GFx::Value(static_cast<double>(slider->scale)));
                                row.SetMember("sliderSteps", RE::Scaleform::GFx::Value(static_cast<double>(slider->steps)));
                            }
                        } else if (const auto* text = std::get_if<StringDefinition>(&setting.definition)) {
                            Text(row, "type", "string");
                            Text(row, "value", std::get<std::string>(value->second));
                            Text(row, "defaultValue", text->defaultValue);
                            row.SetMember("maxLength", RE::Scaleform::GFx::Value(text->maxLength));
                            row.SetMember("editable", RE::Scaleform::GFx::Value(true));
                        } else if (const auto* binding = std::get_if<KeyDefinition>(&setting.definition)) {
                            Text(row, "type", "key");
                            const auto keyCode = std::get<KeyBinding>(value->second).keyCode;
                            row.SetMember("value", RE::Scaleform::GFx::Value(static_cast<double>(keyCode)));
                            row.SetMember("defaultValue", RE::Scaleform::GFx::Value(static_cast<double>(binding->defaultValue.keyCode)));
                            Text(row, "valueName", KeyName(keyCode));
                            Text(row, "defaultName", KeyName(binding->defaultValue.keyCode));
                            row.SetMember("editable", RE::Scaleform::GFx::Value(true));
                            row.SetMember("allowUnbound", RE::Scaleform::GFx::Value(binding->allowUnbound));
                        } else if (const auto* enumeration = std::get_if<EnumDefinition>(&setting.definition)) {
                            Text(row, "type", "enum");
                            Text(row, "value", std::get<EnumValue>(value->second).value);
                            Text(row, "defaultValue", enumeration->defaultValue.value);
                            row.SetMember("editable", RE::Scaleform::GFx::Value(enumeration->options.size() > 1));
                            RE::Scaleform::GFx::Value options;
                            root->CreateArray(&options);
                            for (const auto& option : enumeration->options) {
                                RE::Scaleform::GFx::Value choice;
                                root->CreateObject(&choice);
                                Text(choice, "value", option.value);
                                Text(choice, "label", option.label);
                                options.PushBack(choice);
                            }
                            row.SetMember("options", options);
                        }
                        params.ret->PushBack(row);
                    }
                    for (const auto& hotkey : mod.schema.hotkeys) {
                        if (hotkey.group != group.id) continue;
                        const auto event = NativeHotkeys::EventName(mod.schema.id, hotkey.id);
                        auto row = modRow(mod, group, hotkey.id, hotkey.label);
                        Text(row, "action", event);
                        row.SetMember("registered", RE::Scaleform::GFx::Value(NativeHotkeys::FindAction(event) != nullptr));
                        Text(row, "type", "hotkey");
                        Text(row, "hint", tr("bindings.editHint"));
                        Text(row, "defaultName", hotkey.defaultKey.value_or(tr("values.unboundTitle")));
                        // Current bindings arrive through vanilla ControlBindingsData in the movie.
                        params.ret->PushBack(row);
                    }
                }
            }
            for (const auto& destination : LauncherService::Get().Snapshot()) {
                RE::Scaleform::GFx::Value row;
                root->CreateObject(&row);
                Text(row, "mod", destination.mod);
                Text(row, "modTitle", destination.modTitle);
                Text(row, "key", destination.id);
                Text(row, "title", destination.title);
                Text(row, "type", "launcher");
                row.SetMember("recentOrder", RE::Scaleform::GFx::Value(static_cast<double>(destination.recentOrder)));
                const bool registered = MenuRegistered(destination);
                const bool available = destination.available && registered;
                Text(row, "hint", destination.description);
                Text(row, "message", !registered ? "The owning mod has not registered this menu." : destination.reason);
                row.SetMember("editable", RE::Scaleform::GFx::Value(available));
                params.ret->PushBack(row);
            }
            break;
        }
        case Function::SetBool:
        case Function::SetInt:
        case Function::SetFloat:
        case Function::SetEnum:
        case Function::SetString:
        case Function::SetKey: {
            auto result = SettingsError::InvalidArgument;
            if (params.argCount == (function == Function::SetString ? 4u : 3u) && params.args[0].IsString() && params.args[1].IsString()) {
                if (function == Function::SetBool && params.args[2].IsBoolean()) {
                    result = runtime.SetValue(ArgString(params, 0), ArgString(params, 1), params.args[2].GetBoolean());
                } else if (function == Function::SetInt && params.args[2].IsString()) {
                    if (const auto value = ParseInteger(ArgString(params, 2))) {
                        result = runtime.SetValue(ArgString(params, 0), ArgString(params, 1), *value);
                    }
                } else if (function == Function::SetFloat && params.args[2].IsNumber()) {
                    result = runtime.SetValue(ArgString(params, 0), ArgString(params, 1), params.args[2].GetNumber());
                } else if (function == Function::SetEnum && params.args[2].IsString()) {
                    result = runtime.SetValue(ArgString(params, 0), ArgString(params, 1), EnumValue{ ArgString(params, 2) });
                } else if (function == Function::SetString && params.args[2].IsString() && params.args[3].IsNumber()) {
                    // GFx exposes a C string. The movie also supplies its UTF-8 byte count so embedded NUL cannot turn a rejected draft into a prefix.
                    auto text = ArgString(params, 2);
                    if (params.args[3].GetNumber() == static_cast<double>(text.size())) {
                        result = runtime.SetValue(ArgString(params, 0), ArgString(params, 1), std::move(text));
                    }
                } else if (function == Function::SetKey && params.args[2].IsNumber()) {
                    const auto code = params.args[2].GetNumber();
                    if (std::isfinite(code) && code == std::floor(code) && code >= 0 && code <= KeyBinding::Unbound) {
                        result = runtime.SetValue(ArgString(params, 0), ArgString(params, 1), KeyBinding{ static_cast<std::uint32_t>(code) });
                    }
                }
            }
            root->CreateObject(params.ret);
            const bool ok = result == SettingsError::None;
            params.ret->SetMember("ok", RE::Scaleform::GFx::Value(ok));
            // Detailed file errors go to the log. The player gets an actionable message.
            Text(*params.ret, "error", ok ? "" : tr("errors.save"));
            break;
        }
        }
    }

    bool OSFSettingsMenu::WantsMovieEventForward(const RE::InputEvent* event)
    {
        if (!event) return false;
        if (event->eventType == RE::InputEvent::EventType::kChar) return m_textInputActive;
        if (m_textInputActive && event->eventType == RE::InputEvent::EventType::kButton && event->deviceType == RE::InputEvent::DeviceType::kKeyboard) {
            // Native movie forwarding synthesizes Enter for the underlying gameplay Activate action even when the text context disables it.
            const auto* button = static_cast<const RE::ButtonEvent*>(event);
            if (button->disabled && button->strUserEvent == "Activate") return false;
        }
        return RE::GameMenuBase::WantsMovieEventForward(event);
    }

    bool OSFSettingsMenu::ShouldHandleEvent(const RE::InputEvent* event)
    {
        const auto bindingInput = m_bindingEditor.ProcessInput(event);
        if (bindingInput == NativeBindingEditor::InputResult::Cancelled) menuObj.Invoke("onNativeBindingCancelled");
        if (bindingInput != NativeBindingEditor::InputResult::Unhandled) return false;
        if (event && event->deviceType == RE::InputEvent::DeviceType::kMouse && event->eventType == RE::InputEvent::EventType::kButton &&
            m_capture.ShouldConsumeMouse(MouseVirtualKey(static_cast<std::uint32_t>(static_cast<const RE::ButtonEvent*>(event)->idCode)))) return true;
        if (event && event->deviceType == RE::InputEvent::DeviceType::kKeyboard && event->eventType == RE::InputEvent::EventType::kButton &&
            m_capture.ShouldConsumeKey(static_cast<std::uint32_t>(static_cast<const RE::ButtonEvent*>(event)->idCode))) return true;
        return RE::GameMenuBase::ShouldHandleEvent(event);
    }

    // Keyboard idCode is a Win32 virtual-key code, passed through without scan-code conversion.
    void OSFSettingsMenu::OnButtonEvent(const RE::ButtonEvent* event)
    {
        TestHarness::ObserveInput(event, false);
        if (event && event->deviceType == RE::InputEvent::DeviceType::kMouse &&
            m_capture.ShouldConsumeMouse(MouseVirtualKey(static_cast<std::uint32_t>(event->idCode))) &&
            m_capture.HandleKeyEvent(MouseVirtualKey(static_cast<std::uint32_t>(event->idCode)), event->value > 0, event->heldDownSecs > 0)) return;
        if (event && event->deviceType == RE::InputEvent::DeviceType::kKeyboard &&
            m_capture.HandleKeyEvent(static_cast<std::uint32_t>(event->idCode), event->value > 0, event->heldDownSecs > 0)) return;
        RE::GameMenuBase::OnButtonEvent(event);
    }

    void OSFSettingsMenu::OnRemovedFromMenuStack()
    {
        REX::INFO("LaunchTrace: Settings removed from menu stack (request={}, handoff={})", m_launch ? m_launch->requestId : 0, m_launch && m_launch->closing);
        ++*m_textRequests;
        m_textInputActive = false;
        m_bindings->Invalidate();
        m_bindingEditor.End(true);
        m_capture.ResetForMenuClose();
        RE::GameMenuBase::OnRemovedFromMenuStack();
        auto* ui = RE::UI::GetSingleton();
        if (!m_launch) return;
        if (!m_launch->closing || !ui || ui->IsMenuOpen("MainMenu") || ui->IsMenuOpen("LoadingMenu")) {
            AbandonLaunch();
            return;
        }
        auto launch = std::exchange(m_launch, std::nullopt);
        LauncherService::Get().EndOpen(launch->requestId);
        const auto& destination = launch->destination;
        if (launch->afterClose) {
            // Queue only: the provider consumes this after the native message pump,
            // including CursorMenu hides produced by Settings/Pause removal.
            launch->afterClose(destination.mod, destination.id, launch->requestId);
            LauncherService::Get().RecordOpened(destination.mod, destination.id);
        } else if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            queue->AddMessage(RE::BSFixedString(destination.menu.c_str()), RE::UI_MESSAGE_TYPE::kShow);
            LauncherService::Get().RecordOpened(destination.mod, destination.id);
        }
    }

    RE::Scaleform::Ptr<RE::IMenu> OSFSettingsMenu::Create()
    {
        return RE::Scaleform::Ptr<RE::IMenu>{ new OSFSettingsMenu() };
    }

    bool OSFSettingsMenu::Register()
    {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            return false;
        }
        const RE::BSFixedString name(MENU_NAME.data());
        if (!ui->IsMenuRegistered(name)) {
            ui->RegisterMenu(MENU_NAME.data(), &Create, true);
        }
        const bool registered = ui->IsMenuRegistered(name);
        TestHarness::RegisterMenuObserver(*ui, registered);
        return registered;
    }

    void OSFSettingsMenu::Open() 
    { 
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            queue->AddMessage(RE::BSFixedString(MENU_NAME.data()), RE::UI_MESSAGE_TYPE::kShow); 
        }
    }

    void OSFSettingsMenu::Close()
    {
        if (m_launch && !m_launch->closing) {
            AbandonLaunch();
            CloseWithPause();
            return;
        }
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            queue->AddMessage(RE::BSFixedString(MENU_NAME.data()), RE::UI_MESSAGE_TYPE::kHide);
        }
    }

    void OSFSettingsMenu::AbandonLaunch()
    {
        auto launch = std::exchange(m_launch, std::nullopt);
        if (!launch || !launch->requestId) return;
        LauncherService::Get().EndOpen(launch->requestId);
        const auto& target = launch->destination;
        REX::INFO("LaunchTrace: open abandoned request={} view='{}/{}'", launch->requestId, target.mod, target.id);
    }

    void OSFSettingsMenu::CommitLaunch()
    {
        m_launch->closing = true;
        CloseWithPause();
    }

    void OSFSettingsMenu::CloseWithPause()
    {
        auto* ui = RE::UI::GetSingleton();
        auto* queue = RE::UIMessageQueue::GetSingleton();
        REX::INFO("LaunchTrace: requesting Settings close (PauseMenu={})", ui && ui->IsMenuOpen("PauseMenu"));
        if (ui && queue && ui->IsMenuOpen("PauseMenu")) {
            queue->AddMessage(RE::BSFixedString("PauseMenu"), RE::UI_MESSAGE_TYPE::kHide);
        }
        Close();
    }

}
