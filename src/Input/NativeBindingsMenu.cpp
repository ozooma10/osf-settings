#include "NativeBindingsMenu.h"
#include "NativeHotkeys.h"
#include "Settings/SettingsService.h"
#include "Settings/Localization.h"
#include "RE/B/BSScaleformManager.h"
#include "RE/B/BSScaleformTranslator.h"
#include "RE/S/SettingsDataModel.h"
#include "REL/THook.h"
#include "REX/CONVERT.h"
#include "Harness/TranslationRegistration.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>

namespace OSFSettings::NativeBindingsMenu
{
    namespace
    {
        constexpr std::ptrdiff_t kVisitCall = 0xBA;
        constexpr std::ptrdiff_t kContextNameCall = 0xCE;
        constexpr std::ptrdiff_t kLoadTranslationsCall = 0x4C6;
        constexpr const char* kModContextName = "OSFModBindings";
        using Context = RE::ControlMap::InputContextID;
        using BindingDefinition = RE::SettingsDataModel::BindingDefinition;
        using Translator = RE::BSScaleformTranslator;

        struct VisitorState
        {
            void** model;
            Context* previousContext;
            const bool* gamepad;
        };
        static_assert(sizeof(VisitorState) == 0x18);

        using VisitHook = REL::THook<std::uint32_t(VisitorState*, const BindingDefinition*)>;
        using ContextNameHook = REL::THook<void(RE::BSStringPool::Entry*&, const char*, bool)>;
        using LoadTranslationsHook = REL::THook<void(Translator*, Translator::StreamParser*)>;
        std::optional<VisitHook> g_visitHook;
        std::optional<ContextNameHook> g_contextNameHook;
        std::optional<LoadTranslationsHook> g_loadTranslationsHook;
        std::map<std::string, std::size_t, std::less<>> g_order;
        std::map<std::wstring, std::wstring, std::less<>> g_labels;
        std::vector<Translator::Translation> g_translations;
        thread_local bool g_modRow = false;

        void BuildLabels(std::vector<ModSettings> mods)
        {
            const auto titleKey = [](const ModSettings& mod) {
                auto title = mod.schema.title;
                std::ranges::transform(title, title.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return std::pair{ std::move(title), mod.schema.id };
            };
            std::ranges::sort(mods, {}, titleKey);
            g_order.clear();
            g_translations.clear();
            g_labels.clear();
            for (const auto& mod : mods) {
                for (const auto& hotkey : mod.schema.hotkeys) {
                    const auto event = mod.schema.id + "/" + hotkey.id;
                    if (!NativeHotkeys::FindAction(event)) continue;
                    std::wstring name, label;
                    if (!REX::UTF8_TO_UTF16(event, name) || !REX::UTF8_TO_UTF16(tr("bindings.actionLabel", {{"mod", mod.schema.title}, {"action", hotkey.label}}), label)) continue;
                    g_order.emplace(event, g_order.size());
                    // Original context tokens also occur in native conflict text.
                    for (const auto* context : { L"MainGameplay", L"OSFModBindings" }) {
                        const auto token = std::wstring(L"$") + context + L"_" + name;
                        g_labels.emplace(token, label);
                        g_labels.emplace(token + L"_KBM", label);
                        g_labels.emplace(token + L"_GP", label);
                    }
                }
            }
            std::wstring heading;
            REX::UTF8_TO_UTF16(tr("bindings.heading"), heading);
            g_labels.emplace(L"$OSFModBindings", std::move(heading));
            for (const auto& [key, value] : g_labels) {
                g_translations.push_back({ key.c_str(), value.c_str() });
            }
        }

        void ContextName(RE::BSStringPool::Entry*& result, const char* name, bool caseSensitive)
        {
            (*g_contextNameHook)(result, g_modRow ? kModContextName : name, caseSensitive);
        }

        void LoadTranslations(Translator* translator, Translator::StreamParser* parser)
        {
            TestHarness::BeforeTranslationLoad(translator);
            (*g_loadTranslationsHook)(translator, parser);
            Localization::Initialize();
            BuildLabels(SettingsService::Get().Snapshot());
            TestHarness::BeforeTranslationRegistration(translator, g_labels);
            const bool registered = translator->RegisterTranslations(g_translations);
            if (!registered) REX::ERROR("Mod bindings translation registration failed");
            TestHarness::AfterTranslationRegistration(translator, registered);
        }

        std::uint32_t VisitBindings(VisitorState* state, const BindingDefinition* first)
        {
            const auto definitions = RE::SettingsDataModel::GetBindingDefinitions();
            if (g_order.empty() || first != definitions.data()) {
                return (*g_visitHook)(state, first);
            }

            std::vector<std::pair<std::size_t, const BindingDefinition*>> mods;
            mods.reserve(g_order.size());
            for (const auto& row : definitions) {
                const auto found = g_order.find(std::string_view(row.event));
                if (row.context == Context::kMainGameplay && found != g_order.end()) {
                    mods.emplace_back(found->second, &row);
                }
            }
            std::ranges::stable_sort(mods, {}, &decltype(mods)::value_type::first);

            if (!mods.empty()) {
                struct ModRows
                {
                    bool previous = std::exchange(g_modRow, true);
                    ~ModRows() { g_modRow = previous; }
                } scope;
                for (const auto& [order, row] : mods) {
                    if ((*g_visitHook)(state, row) != 1) return 0;
                }
                // Mod rows also use MainGameplay; force its vanilla divider next.
                *state->previousContext = Context::kCount;
            }
            for (const auto& row : definitions) {
                if (row.context == Context::kMainGameplay && g_order.contains(std::string_view(row.event))) continue;
                if ((*g_visitHook)(state, &row) != 1) return 0;
            }
            // The enclosing native loop stops when its visitor returns != 1; all rows were emitted above, so it must not visit them again.
            return 0;
        }

        bool InstallHooks()
        {
            g_visitHook.emplace("ModBindings::Rows", RE::ID::SettingsDataModel::PublishBindings, kVisitCall, &VisitBindings);
            g_contextNameHook.emplace("ModBindings::ContextName", RE::ID::SettingsDataModel::PublishBinding, kContextNameCall, &ContextName);
            g_loadTranslationsHook.emplace("ModBindings::Translations", RE::ID::BSScaleformManager::Ctor, kLoadTranslationsCall, &LoadTranslations);
            if (!g_visitHook->Init() || !g_contextNameHook->Init() || !g_loadTranslationsHook->Init()) {
                g_visitHook.reset();
                g_contextNameHook.reset();
                g_loadTranslationsHook.reset();
                return false;
            }
            g_loadTranslationsHook->Enable();
            g_contextNameHook->Enable();
            g_visitHook->Enable();
            return true;
        }
    }

    bool Install()
    {
        if (g_visitHook) {
            return g_visitHook->GetEnabled();
        }

        BuildLabels(SettingsService::Get().Snapshot());
        if (RE::BSScaleformManager::GetSingleton()) {
            REX::ERROR("Mod bindings presentation must install before Scaleform initialization");
            return false;
        }
        if (!InstallHooks()) return false;
        REX::INFO("Mod bindings presentation installed for {} actions", g_order.size());
        return true;
    }
}
