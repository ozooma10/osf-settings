#include "OSFSettingsMenu.h"
#include "Core/Runtime.h"
#include "RE/U/UI.h"
#include "RE/U/UIMessageQueue.h"

namespace OSFSettings
{
    namespace
    {
        enum class Function : std::uintptr_t { GetRows = 1, SetBool, Close, Startup, StartupFailed };

        std::string ArgString(const RE::Scaleform::GFx::FunctionHandler::Params& params, std::uint32_t index)
        {
            return index < params.argCount && params.args[index].IsString() ? params.args[index].GetString() : "";
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
        RegisterNativeFunction("close", static_cast<std::uint64_t>(Function::Close));
        RegisterNativeFunction("startup", static_cast<std::uint64_t>(Function::Startup));
        RegisterNativeFunction("startupFailed", static_cast<std::uint64_t>(Function::StartupFailed));
    }

    void OSFSettingsMenu::OnStartupFailed(std::string_view message)
    {
        REX::ERROR("OSF Settings menu startup failed: {}", message);
        Close();
    }

    void OSFSettingsMenu::Call(const RE::Scaleform::GFx::FunctionHandler::Params& params)
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
            for (const auto& mod : runtime.Settings().Mods()) {
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
                        row.SetMember("value", RE::Scaleform::GFx::Value(value->second));
                        row.SetMember("defaultValue", RE::Scaleform::GFx::Value(setting.defaultValue));
                        params.ret->PushBack(row);
                    }
                }
            }
            break;
        case Function::SetBool: {
            const bool valid = params.argCount == 3 && params.args[0].IsString() && params.args[1].IsString() && params.args[2].IsBoolean();
            const auto result = valid ? runtime.SetBool(ArgString(params, 0), ArgString(params, 1), params.args[2].GetBoolean()) : SettingsStore::SetResult{ false, "invalid edit arguments" };
            root->CreateObject(params.ret);
            params.ret->SetMember("ok", RE::Scaleform::GFx::Value(result.ok));
            // Detailed file errors go to the log. The player gets an actionable message.
            Text(*params.ret, "error", result.ok ? "" : "Could not save this setting. Your previous value is unchanged.");
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
