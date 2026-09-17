#include "OSFSettingsMenu.h"
#include "harness/TestHarness.h"
#include "FloatSlider.h"
#include "Input/KeyNames.h"
#include <cmath>
#include "Core/Runtime.h"
#include <charconv>
#include "RE/U/UI.h"
#include "RE/U/UIMessageQueue.h"

namespace OSFSettings
{
    namespace
    {
        enum class Function : std::uintptr_t { GetRows = 1, SetBool, SetInt, SetFloat, SetEnum, Close, Startup, StartupFailed,
            SetKey, BeginKeyCapture, PollKeyCapture, CommitKeyCapture, CancelKeyCapture };

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
    }

    OSFSettingsMenu::OSFSettingsMenu()
    {
        menuName = MENU_NAME.data();
        SetFlags(RE::IMenu::kPausesGame | RE::IMenu::kBlocksLowerMenuInput | RE::IMenu::ShowCursor | RE::IMenu::kModal);
    }

    void OSFSettingsMenu::PostCreate()
    {
        // Use PauseMenu's native input contexts, with priority one above Pause (0x0B).
        depthPriority = 0x0C;
        for (const auto context : { InputContextID::kBasicMenuNav, InputContextID::kLeftThumbstick, InputContextID::kVirtualController }) {
            AddInputContext(context);
        }
    }

    void OSFSettingsMenu::MapCodeObjectFunctions()
    {
        RegisterNativeFunction("getRows", static_cast<std::uint64_t>(Function::GetRows));
        RegisterNativeFunction("setBool", static_cast<std::uint64_t>(Function::SetBool));
        RegisterNativeFunction("setInt", static_cast<std::uint64_t>(Function::SetInt));
        RegisterNativeFunction("setFloat", static_cast<std::uint64_t>(Function::SetFloat));
        RegisterNativeFunction("setEnum", static_cast<std::uint64_t>(Function::SetEnum));
        RegisterNativeFunction("setKey", static_cast<std::uint64_t>(Function::SetKey));
        RegisterNativeFunction("beginKeyCapture", static_cast<std::uint64_t>(Function::BeginKeyCapture));
        RegisterNativeFunction("pollKeyCapture", static_cast<std::uint64_t>(Function::PollKeyCapture));
        RegisterNativeFunction("commitKeyCapture", static_cast<std::uint64_t>(Function::CommitKeyCapture));
        RegisterNativeFunction("cancelKeyCapture", static_cast<std::uint64_t>(Function::CancelKeyCapture));
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
            const auto value = SettingsService::Get().GetValue(mod, key);
            const bool ok = m_capture.GetSnapshot().state == KeyCapture::State::Idle && value && std::holds_alternative<KeyBinding>(*value);
            if (ok) {
                m_captureMod = mod;
                m_captureKey = key;
                m_capture.BeginCapture();
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
            params.ret->SetMember("keyCode", RE::Scaleform::GFx::Value(static_cast<double>(snapshot.selectedKeyCode)));
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
            Text(*params.ret, "error", ok ? "" : "Could not save this setting. Your previous value is unchanged.");
            break;
        }
        case Function::CancelKeyCapture:
            m_capture.EndCapture();
            break;
        case Function::GetRows:
            root->CreateArray(params.ret);
            for (const auto& mod : runtime.Settings()) {
                for (const auto& group : mod.schema.groups) {
                    for (const auto& setting : group.settings) {
                        const auto value = mod.values.find(setting.key);
                        if (value == mod.values.end()) continue;
                        RE::Scaleform::GFx::Value row;
                        root->CreateObject(&row);
                        Text(row, "mod", mod.schema.id);
                        Text(row, "modTitle", mod.schema.title);
                        Text(row, "modDescription", mod.schema.description);
                        Text(row, "group", group.id);
                        Text(row, "groupTitle", group.label);
                        Text(row, "key", setting.key);
                        Text(row, "title", setting.label);
                        Text(row, "hint", setting.hint);
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
                            constexpr std::int64_t safeInteger = 9007199254740991LL;
                            const bool editable = integer.minimum && integer.maximum && *integer.minimum < *integer.maximum &&
                                *integer.minimum >= -safeInteger && *integer.maximum <= safeInteger &&
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
                            Text(row, "value", std::get<std::string>(value->second));
                            Text(row, "defaultValue", enumeration->defaultValue);
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
                }
            }
            break;
        case Function::SetBool:
        case Function::SetInt:
        case Function::SetFloat:
        case Function::SetEnum:
        case Function::SetKey: {
            auto result = SettingsError::InvalidArgument;
            if (params.argCount == 3 && params.args[0].IsString() && params.args[1].IsString()) {
                if (function == Function::SetBool && params.args[2].IsBoolean()) {
                    result = runtime.SetValue(ArgString(params, 0), ArgString(params, 1), params.args[2].GetBoolean());
                } else if (function == Function::SetInt && params.args[2].IsString()) {
                    if (const auto value = ParseInteger(ArgString(params, 2))) {
                        result = runtime.SetValue(ArgString(params, 0), ArgString(params, 1), *value);
                    }
                } else if (function == Function::SetFloat && params.args[2].IsNumber()) {
                    result = runtime.SetValue(ArgString(params, 0), ArgString(params, 1), params.args[2].GetNumber());
                } else if (function == Function::SetEnum && params.args[2].IsString()) {
                    result = runtime.SetValue(ArgString(params, 0), ArgString(params, 1), ArgString(params, 2));
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
            Text(*params.ret, "error", ok ? "" : "Could not save this setting. Your previous value is unchanged.");
            break;
        }
        }
    }

    bool OSFSettingsMenu::ShouldHandleEvent(const RE::InputEvent* event)
    {
        if (event && event->deviceType == RE::InputEvent::DeviceType::kKeyboard && event->eventType == RE::InputEvent::EventType::kButton &&
            m_capture.ShouldConsumeKey(static_cast<std::uint32_t>(static_cast<const RE::ButtonEvent*>(event)->idCode))) return true;
        return RE::GameMenuBase::ShouldHandleEvent(event);
    }

    // Keyboard idCode is a Win32 virtual-key code, passed through without scan-code conversion.
    void OSFSettingsMenu::OnButtonEvent(const RE::ButtonEvent* event)
    {
        TestHarness::ObserveInput(event, false);
        if (event && event->deviceType == RE::InputEvent::DeviceType::kKeyboard &&
            m_capture.HandleKeyEvent(static_cast<std::uint32_t>(event->idCode), event->value > 0, event->heldDownSecs > 0)) return;
        RE::GameMenuBase::OnButtonEvent(event);
    }

    void OSFSettingsMenu::OnRemovedFromMenuStack()
    {
        m_capture.ResetForMenuClose();
        RE::GameMenuBase::OnRemovedFromMenuStack();
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
        m_capture.EndCapture();
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            queue->AddMessage(RE::BSFixedString(MENU_NAME.data()), RE::UI_MESSAGE_TYPE::kHide); 
        }
    }
}
