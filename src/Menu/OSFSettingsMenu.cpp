#include "OSFSettingsMenu.h"
#include "FloatSlider.h"
#include "Core/Runtime.h"
#include <charconv>
#include "RE/U/UI.h"
#include "RE/U/UIMessageQueue.h"

namespace OSFSettings
{
    namespace
    {
        enum class Function : std::uintptr_t { GetRows = 1, SetBool, SetInt, SetFloat, SetEnum, Close, Startup, StartupFailed };

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
        RegisterNativeFunction("close", static_cast<std::uint64_t>(Function::Close));
        RegisterNativeFunction("startup", static_cast<std::uint64_t>(Function::Startup));
        RegisterNativeFunction("startupFailed", static_cast<std::uint64_t>(Function::StartupFailed));
    }

    void OSFSettingsMenu::OnStartupFailed(std::string_view message)
    {
        REX::ERROR("OSF Settings menu startup failed: {}", message);
        Close();
    }

    void OSFSettingsMenu::Call(const RE::Scaleform::GFx::FunctionHandler::Params& params) noexcept
    {
        if (!params.ret || !params.movie || !params.movie->asMovieRoot) return;
        
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
        case Function::SetEnum: {
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
        return ui->IsMenuRegistered(name);
    }

    void OSFSettingsMenu::Open() 
    { 
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            queue->AddMessage(RE::BSFixedString(MENU_NAME.data()), RE::UI_MESSAGE_TYPE::kShow); 
        }
    }

    void OSFSettingsMenu::Close() 
    { 
        if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
            queue->AddMessage(RE::BSFixedString(MENU_NAME.data()), RE::UI_MESSAGE_TYPE::kHide); 
        }
    }
}
