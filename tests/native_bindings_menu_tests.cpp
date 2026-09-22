#include "SFSE/Impl/PCH.h"
#include "../src/Input/NativeBindingsMenu.cpp"

#include <iostream>
#include <set>
#include <stdexcept>

namespace
{
    using namespace OSFSettings::NativeBindingsMenu;
    std::byte* publisher{};
    std::byte* visitor{};
    std::byte* manager{};
    struct DefinitionStorage
    {
        std::uint32_t size{}, capacity{};
        const BindingDefinition* data{};
    } definitions;
    static_assert(sizeof(definitions) == sizeof(RE::BSTArray<BindingDefinition>));
    std::vector<const BindingDefinition*> emitted;
    std::vector<std::string> dividers;
    std::vector<std::string> contexts;
    std::set<std::string> registered;
    std::map<std::wstring, std::wstring> loadedLabels;
    unsigned stockLoads{};
    bool registeredAfterStock{};
    bool stopVisitor{};
    std::vector<OSFSettings::ModSettings> sourceMods;
    unsigned localizationInitializations{};

    template<class Char>
    void GetString(RE::BSStringPool::Entry*& result, const Char* text, bool)
    {
        const auto bytes = std::char_traits<Char>::length(text) * sizeof(Char);
        auto* storage = new std::byte[sizeof(RE::BSStringPool::Entry) + bytes + sizeof(Char)]{};
        auto* entry = std::construct_at(reinterpret_cast<RE::BSStringPool::Entry*>(storage));
        entry->_length = static_cast<std::uint32_t>(bytes);
        entry->_refCount = 1;
        std::memcpy(entry + 1, text, bytes + sizeof(Char));
        result = entry;
    }

    void ReleaseString(RE::BSStringPool::Entry*& entry)
    {
        if (entry && --entry->_refCount == 0) delete[] reinterpret_cast<std::byte*>(entry);
        entry = nullptr;
    }

    void NativeLoad(Translator*, Translator::StreamParser*)
    {
        ++stockLoads;
        loadedLabels[L"$MainGameplay_Forward_KBM"] = L"Vanilla translation";
    }

    std::uint32_t NativeVisit(VisitorState* state, const BindingDefinition* row)
    {
        RE::BSStringPool::Entry* name{};
        // Execute the patched context-construction CALL at its native offset.
        const auto context = reinterpret_cast<void (*)(RE::BSStringPool::Entry*&, const char*, bool)>(visitor + kContextNameCall - 4);
        context(name, row->context == Context::kMainGameplay ? "MainGameplay" : "ShipHUD", true);
        const std::string text(name->data<char>());
        ReleaseString(name);
        if (*state->previousContext != row->context) {
            dividers.push_back(text);
            *state->previousContext = row->context;
        }
        contexts.push_back(text);
        emitted.push_back(row);
        return stopVisitor ? 0 : 1;
    }

    void WriteCall(std::byte* code, std::size_t offset, std::uintptr_t target)
    {
        const REL::ASM::CALL5 call{ reinterpret_cast<std::uintptr_t>(code + offset), target };
        std::memcpy(code + offset, &call, sizeof(call));
    }

    std::byte* Caller(std::size_t offset, std::uintptr_t target)
    {
        auto* code = static_cast<std::byte*>(REL::GetTrampoline().allocate(0x600));
        std::memset(code, 0x90, 0x600);
        std::memcpy(code, "\x48\x83\xEC\x28", 4);
        WriteCall(code, offset, target);
        std::memcpy(code + offset + 5, "\x48\x83\xC4\x28\xC3", 5);
        return code;
    }

    struct Fixture
    {
        ~Fixture()
        {
            const auto remove = [](auto& hook) {
                if (hook && hook->GetEnabled()) hook->Disable();
                hook.reset();
            };
            remove(g_visitHook);
            remove(g_contextNameHook);
            remove(g_loadTranslationsHook);
        }
    };
}

namespace OSFSettings
{
    SettingsService& SettingsService::Get() { static SettingsService service; return service; }
    std::vector<ModSettings> SettingsService::Snapshot() const { return sourceMods; }
    namespace Localization { void Initialize() { ++localizationInitializations; } }
    namespace NativeHotkeys
    {
        const Action* FindAction(std::string_view event)
        {
            static Action action;
            return registered.contains(std::string(event)) ? &action : nullptr;
        }
    }
}

namespace RE
{
    bool BSScaleformTranslator::RegisterTranslations(std::span<const Translation> translations)
    {
        registeredAfterStock = stockLoads > 0;
        for (const auto& translation : translations) loadedLabels[translation.key] = translation.value;
        return true;
    }
}

namespace REL
{
    IDDB::IDDB() = default;
    std::uint64_t IDDB::offset(std::uint64_t id) const
    {
        std::uintptr_t address{};
        switch (id) {
        case 88747: address = reinterpret_cast<std::uintptr_t>(publisher); break;
        case 88748: address = reinterpret_cast<std::uintptr_t>(visitor); break;
        case 918356: address = reinterpret_cast<std::uintptr_t>(&definitions); break;
        case 130697: address = reinterpret_cast<std::uintptr_t>(manager); break;
        case 130926: address = reinterpret_cast<std::uintptr_t>(&NativeLoad); break;
        default:
            if (id == RE::ID::BSStringPool::GetEntry.id() || id == 1186742) address = reinterpret_cast<std::uintptr_t>(&GetString<char>);
            else if (id == RE::ID::BSStringPool::GetEntryW.id()) address = reinterpret_cast<std::uintptr_t>(&GetString<wchar_t>);
            else if (id == RE::ID::BSStringPool::Entry::Release.id()) address = reinterpret_cast<std::uintptr_t>(&ReleaseString);
            else throw std::runtime_error("Unexpected relocation: " + std::to_string(id));
        }
        return address - REX::FModule::GetExecutingModule().GetBaseAddress();
    }
}

int main()
{
    unsigned checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    try {
        REL::GetTrampoline().create(8192);
        visitor = Caller(kContextNameCall, reinterpret_cast<std::uintptr_t>(&GetString<char>));
        const REL::ASM::JMP5 jump{ reinterpret_cast<std::uintptr_t>(visitor), reinterpret_cast<std::uintptr_t>(&NativeVisit) };
        std::memcpy(visitor, &jump, sizeof(jump));
        std::memcpy(visitor + kContextNameCall - 4, "\x48\x83\xEC\x28", 4);
        publisher = Caller(kVisitCall, reinterpret_cast<std::uintptr_t>(visitor));
        manager = Caller(kLoadTranslationsCall, reinterpret_cast<std::uintptr_t>(&NativeLoad));
        Fixture fixture;

        for (auto* call : { publisher + kVisitCall, visitor + kContextNameCall, manager + kLoadTranslationsCall }) {
            const auto savedOpcode = *call;
            *call = std::byte{0x90};
            check(!InstallHooks() && !g_visitHook && !g_contextNameHook && !g_loadTranslationsHook,
                "initialization failure clears every hook for retry");
            *call = savedOpcode;
            check(REL::ASM::CALL5::TARGET(reinterpret_cast<std::uintptr_t>(publisher + kVisitCall)) == reinterpret_cast<std::uintptr_t>(visitor) &&
                REL::ASM::CALL5::TARGET(reinterpret_cast<std::uintptr_t>(visitor + kContextNameCall)) == reinterpret_cast<std::uintptr_t>(&GetString<char>) &&
                REL::ASM::CALL5::TARGET(reinterpret_cast<std::uintptr_t>(manager + kLoadTranslationsCall)) == reinterpret_cast<std::uintptr_t>(&NativeLoad),
                "initialization failure leaves every native call untouched");
        }
        check(InstallHooks(), "all three real CALL5 hooks install");

        std::vector<OSFSettings::ModSettings> mods(3);
        mods[0].schema.id = "zeta"; mods[0].schema.title = "Zeta";
        mods[0].schema.hotkeys = {{"open", "Ouvrir réglages", std::nullopt}};
        mods[1].schema.id = "alpha"; mods[1].schema.title = "alpha";
        mods[1].schema.hotkeys = {{"second", "Declared first", std::nullopt}, {"first", "Declared second", std::nullopt}, {"invalid", "Invalid", std::nullopt}};
        mods[2].schema.id = "empty"; mods[2].schema.title = "Empty";
        registered = {"zeta/open", "alpha/second", "alpha/first"};
        sourceMods = mods;
        BuildLabels(mods);
        check(g_order.size() == 3 && g_order.at("alpha/second") == 0 && g_order.at("alpha/first") == 1 && g_order.at("zeta/open") == 2,
            "sort mods by title, preserve declaration order, omit unregistered actions");

        Translator translator{};
        Translator::StreamParser parser{};
        const auto load = reinterpret_cast<void (*)(Translator*, Translator::StreamParser*)>(manager);
        load(&translator, &parser);
        check(stockLoads == 1 && registeredAfterStock, "stock load precedes registration through the patched constructor call");
        check(localizationInitializations == 1, "catalog initialization runs at the constructor hook");
        check(loadedLabels.at(L"$OSFModBindings") == L"Mod Bindings", "section title registered");
        check(loadedLabels.at(L"$OSFModBindings_zeta/open_KBM") == L"Zeta: Ouvrir réglages", "UTF-8 labels become wide strings");
        check(loadedLabels.at(L"$MainGameplay_zeta/open") == L"Zeta: Ouvrir réglages", "original-context conflict label registered");
        check(loadedLabels.at(L"$OSFModBindings_alpha/first_GP") == L"alpha: Declared second", "gamepad label suffix registered");
        check(loadedLabels.at(L"$MainGameplay_Forward_KBM") == L"Vanilla translation", "stock labels remain intact");
        const auto count = loadedLabels.size();
        load(&translator, &parser);
        check(stockLoads == 2 && loadedLabels.size() == count, "constructor replay registers labels without duplicates");

        std::array<BindingDefinition, 6> rows{};
        const char* events[] = {"zeta/open", "alpha/first", "Forward", "alpha/second", "RepairShip", "alpha/second"};
        for (std::size_t i = 0; i < rows.size(); ++i) {
            rows[i].event = events[i];
            rows[i].context = i < 4 ? Context::kMainGameplay : Context::kShipHUD;
            rows[i].variants = reinterpret_cast<void*>(0x1000 + i * 0x40);
        }
        definitions = {static_cast<std::uint32_t>(rows.size()), static_cast<std::uint32_t>(rows.size()), rows.data()};
        std::array<std::byte, sizeof(rows)> before;
        std::memcpy(before.data(), rows.data(), before.size());
        Context previous = Context::kCount;
        void* model{};
        bool gamepad{};
        VisitorState state{&model, &previous, &gamepad};
        const auto publish = reinterpret_cast<std::uint32_t (*)(VisitorState*, const BindingDefinition*)>(publisher);
        check(publish(&state, rows.data()) == 0, "native outer loop stops after the reordered visit");
        check(emitted == std::vector<const BindingDefinition*>{&rows[3], &rows[1], &rows[0], &rows[2], &rows[4], &rows[5]}, "exact registered MainGameplay rows come first; vanilla order remains intact");
        check(dividers == std::vector<std::string>{"OSFModBindings", "MainGameplay", "ShipHUD"}, "exactly one mod divider precedes the intact vanilla sections");
        check(contexts == std::vector<std::string>{"OSFModBindings", "OSFModBindings", "OSFModBindings", "MainGameplay", "ShipHUD", "ShipHUD"}, "only display context changes for mod rows");
        check(!g_modRow && std::memcmp(before.data(), rows.data(), before.size()) == 0, "context IDs, variants and definition storage stay unchanged");
        emitted.clear(); dividers.clear(); contexts.clear(); previous = Context::kCount;
        publish(&state, rows.data());
        check(emitted.size() == rows.size() && dividers.size() == 3, "rebuild does not duplicate rows or headers");

        stopVisitor = true; emitted.clear(); previous = Context::kCount;
        publish(&state, rows.data());
        check(emitted.size() == 1 && !g_modRow, "native early-stop result is respected");
        stopVisitor = false;
        registered.clear(); BuildLabels(mods); emitted.clear(); dividers.clear(); previous = Context::kCount;
        check(publish(&state, rows.data()) == 1 && emitted.size() == 1 && dividers == std::vector<std::string>{"MainGameplay"}, "empty registry leaves the native iteration in charge");
        const auto initialized = localizationInitializations;
        sourceMods.clear();
        load(&translator, &parser);
        check(localizationInitializations == initialized + 1 && g_order.empty() && loadedLabels.at(L"$OSFModBindings") == L"Mod Bindings",
            "localization initializes without any schema or registered hotkey");
        std::cout << checks << " native bindings presentation checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
