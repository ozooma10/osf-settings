#include "SFSE/Impl/PCH.h"
// Include the implementation so this standalone fixture can remove its hook
// before destroying the synthetic loader and CommonLib test-process state.
#include "../src/Input/NativeHotkeys.cpp"
#include "Settings/SettingsService.h"
#include "RE/C/ControlMap.h"
#include "Input/KeyNames.h"
#include "REL/ASM.h"
#include "REL/Trampoline.h"

#include <iostream>
#include <stdexcept>

namespace
{
    std::vector<OSFSettings::ModSettings> schemas;
    constexpr wchar_t keyTable[] = L"F4\t0x73\t115\nF10\t0x79\t121\nL Ctrl\t0xA2\t17\n/\t0xBF";
    std::byte* loader{};
    std::byte* customLoader{};
    RE::ControlMap* parsedMap{};
    std::vector<std::string> parsedTexts;
    std::vector<std::string> customBlocks;
    std::uint8_t customDevice{};
    struct Row
    {
        std::string event;
        std::string key;
        bool nativeFlags;
    };
    std::vector<Row> formattedRows;

    struct HookFixture
    {
        void Reset()
        {
            auto& custom = OSFSettings::NativeHotkeys::g_customBlockHook;
            if (custom) { custom->Disable(); custom.reset(); }
            auto& hook = OSFSettings::NativeHotkeys::g_parseHook;
            if (hook) {
                hook->Disable();
                hook.reset();
            }
        }

        ~HookFixture() { Reset(); }
    };

    void OriginalParser(RE::ControlMap* map, const char* text)
    {
        parsedMap = map;
        parsedTexts.emplace_back(text);
    }
    void OriginalCustom(RE::ControlMap* map, std::uint8_t device, const char* block)
    {
        parsedMap = map; customDevice = device;
        const auto size = (static_cast<unsigned char>(block[1]) << 8) | static_cast<unsigned char>(block[2]);
        customBlocks.emplace_back(block, size);
    }

    std::string SavedKeyboardBlock(std::string_view action)
    {
        std::string block(3, '\0');
        block[0] = 3;
        block += "MainGameplay"; block += '\0';
        block += action; block += '\0';
        block.append("\x01\x00\x00\x00\x7A\x00\x02\x00", 8);
        block[1] = static_cast<char>(block.size() >> 8);
        block[2] = static_cast<char>(block.size() & 0xFF);
        return block;
    }
}

// Supply schema data and engine boundaries; execute the real lookup and hook.
namespace OSFSettings
{
    SettingsService& SettingsService::Get() { static SettingsService service; return service; }
    std::vector<ModSettings> SettingsService::Snapshot() const { return schemas; }
}

namespace RE
{
    std::string ControlMap::FormatMappingRow(const char* event, const char* keyboard,
        const char* mouse, const char* gamepad, bool keyboardVisible, bool mouseVisible, bool gamepadVisible,
        std::uint32_t controlMask, std::uint32_t groupMask, bool required)
    {
        formattedRows.push_back({ event, keyboard, std::string_view(mouse) == "0xff" && std::string_view(gamepad) == "0xff" && keyboardVisible &&
            !mouseVisible && gamepadVisible && controlMask == (std::string_view(event).ends_with("/openMenu") ? 0x08u : 0x401u) && groupMask == 0 && !required });
        return std::format("{}\t{}\t0xff\t0xff\t1\t0\t1\t{:#x}\t0\t0\n", event, keyboard, controlMask);
    }
}

namespace REL
{
    IDDB::IDDB() = default;
    std::uint64_t IDDB::offset(std::uint64_t id) const
    {
        std::uintptr_t address{};
        switch (id) {
        case 124116: address = reinterpret_cast<std::uintptr_t>(loader); break;
        case 124129: address = reinterpret_cast<std::uintptr_t>(customLoader); break;
        case 124127: address = reinterpret_cast<std::uintptr_t>(&OriginalCustom); break;
        case 361050: address = reinterpret_cast<std::uintptr_t>(keyTable); break;
        default: throw std::runtime_error("Unexpected relocation: " + std::to_string(id));
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
        // A small executable loader with the real CALL5 position and x64 ABI.
        auto& trampoline = REL::GetTrampoline();
        trampoline.create(1024);
        loader = static_cast<std::byte*>(trampoline.allocate(0x60));
        std::memset(loader, 0x90, 0x60);
        std::memcpy(loader, "\x48\x83\xEC\x28", 4); // sub rsp, 0x28
        const REL::ASM::CALL5 call{ reinterpret_cast<std::uintptr_t>(loader + 0x45),
            reinterpret_cast<std::uintptr_t>(&OriginalParser) };
        std::memcpy(loader + 0x45, &call, sizeof(call));
        std::memcpy(loader + 0x4A, "\x48\x83\xC4\x28\xC3", 5); // add rsp, 0x28; ret
        const auto load = reinterpret_cast<void (*)(RE::ControlMap*, const char*)>(loader);
        customLoader = static_cast<std::byte*>(trampoline.allocate(0x150));
        std::memset(customLoader, 0x90, 0x150);
        std::memcpy(customLoader, "\x48\x83\xEC\x28", 4);
        const REL::ASM::CALL5 customCall{ reinterpret_cast<std::uintptr_t>(customLoader + 0x12A),
            reinterpret_cast<std::uintptr_t>(&OriginalCustom) };
        std::memcpy(customLoader + 0x12A, &customCall, sizeof(customCall));
        std::memcpy(customLoader + 0x12F, "\x48\x83\xC4\x28\xC3", 5);
        const auto loadCustom = reinterpret_cast<void (*)(RE::ControlMap*, std::uint8_t, const char*)>(customLoader);
        HookFixture fixture;

        check(OSFSettings::GetKeyboardVirtualKey("f10") == 0x79 &&
            OSFSettings::GetKeyboardVirtualKey("L Ctrl") == 0xA2 &&
            OSFSettings::GetKeyboardVirtualKey("/") == 0xBF,
            "native table lookup handles case, spaced names and a final row without newline");
        check(OSFSettings::GetKeyboardVirtualKey("") == 0xFFFFFFFF &&
            OSFSettings::GetKeyboardVirtualKey("Unknown") == 0xFFFFFFFF,
            "unknown names cannot silently become key zero");

        RE::ControlMap first{}, reset{};
        const char vanilla[] = "// MainGameplay\r\nJump\t0x20\t0xff\t0xff\t1\t0\t0\t0x401\t0\t0\r\n\r\n"
            "// ConsoleOpening\r\nConsole\t0xc0\t0xff\t0xff\t0\t0\t0\t0\t0\t0\r\n";
        check(OSFSettings::NativeHotkeys::Install(), "empty declarations need no hook");
        load(&first, vanilla);
        check(parsedTexts.size() == 1 && parsedTexts.back() == vanilla, "empty declarations preserve vanilla input");

        schemas.resize(3);
        schemas[0].schema.id = "examplemods";
        schemas[0].schema.hotkeys = { { "openMenu", "Open settings", "F10", "OSFSettingsMenu" },
            { "unbound", "Unbound", std::nullopt }, { "invalid", "Invalid", "Unknown", "InvalidMenu" },
            { "badCallback", "Invalid callback", "Unknown" } };
        schemas[1].schema.id = "anothermod";
        schemas[1].schema.hotkeys = { { "openMenu", "Other action", "F4", "OtherMenu" },
            { "toggle", "Bound callback", "F4" } };
        schemas[2].schema.id = "empty";
        check(OSFSettings::NativeHotkeys::Install(), "real CALL5 hook installs");
        check(formattedRows.size() == 4 && formattedRows[0].event == "examplemods/openMenu" &&
            formattedRows[0].key == "0x79*0" && formattedRows[1].key == "0xff*0" &&
            formattedRows[2].event == "anothermod/openMenu" && formattedRows[2].key == "0x73*0" &&
            formattedRows[3].event == "anothermod/toggle" && formattedRows[3].key == "0x73*0",
            "bound and unbound actions use explicit primary slots; invalid defaults are excluded");
        const auto legacy = SavedKeyboardBlock("examplemods/openMenu");
        loadCustom(&first, 0, legacy.data());
        auto migrated = legacy; migrated[migrated.size() - 2] = 0;
        check(customBlocks.size() == 1 && customBlocks.back() == migrated && parsedMap == &first && customDevice == 0,
            "a third-party menu binding migrates without any osfsettings action being registered");
        loadCustom(&first, 1, legacy.data());
        loadCustom(&first, 2, legacy.data());
        check(customBlocks.size() == 3 && customBlocks[1] == legacy && customBlocks[2] == legacy && customDevice == 2,
            "mouse and gamepad overlays pass through byte-identically");
        loadCustom(&first, 0, migrated.data());
        check(customBlocks.size() == 4 && customBlocks.back() == migrated,
            "current metadata is idempotent through the hooked loader");
        for (const auto action : {"anothermod/openMenu", "anothermod/toggle", "examplemods/unbound"}) {
            const auto saved = SavedKeyboardBlock(action);
            auto expected = saved; expected[expected.size() - 2] = 0;
            const auto count = customBlocks.size();
            loadCustom(&reset, 0, saved.data());
            check(customBlocks.size() == count + 1 && customBlocks.back() == expected && parsedMap == &reset,
                "each registered menu or callback action migrates through one native call, including unbound defaults");
        }
        for (const auto action : {"examplemods/invalid", "examplemods/badCallback",
                 "unregistered/openMenu", "Forward"}) {
            const auto saved = SavedKeyboardBlock(action);
            const auto count = customBlocks.size();
            loadCustom(&first, 0, saved.data());
            check(customBlocks.size() == count + 1 && customBlocks.back() == saved,
                "invalid, unregistered and vanilla actions pass through unchanged");
        }
        auto clearedAlternate = SavedKeyboardBlock("anothermod/toggle");
        clearedAlternate[clearedAlternate.size() - 4] = static_cast<char>(0xFF);
        clearedAlternate[clearedAlternate.size() - 3] = 2;
        loadCustom(&first, 0, clearedAlternate.data());
        check(customBlocks.back() == clearedAlternate,
            "a cleared alternate of a bound callback cannot clear its default primary");
        auto clearedPrimary = clearedAlternate; clearedPrimary[clearedPrimary.size() - 2] = 0;
        loadCustom(&first, 0, clearedPrimary.data());
        check(customBlocks.back() == clearedPrimary,
            "a current-format primary clear is preserved by the native loading hook");
        check(std::ranges::all_of(formattedRows, [](const Row& row) { return row.nativeFlags; }),
            "menu targets use the menu mask; callback actions retain their gameplay flags");
        using OSFSettings::NativeHotkeys::FindAction;
        const auto* menu = FindAction("examplemods/openMenu");
        const auto* otherMenu = FindAction("anothermod/openMenu");
        const auto* callback = FindAction("examplemods/unbound");
        check(menu && menu->menu == "OSFSettingsMenu" && otherMenu && otherMenu->menu == "OtherMenu" &&
            callback && !callback->menu && callback->event == "examplemods/unbound" &&
            callback->mod == "examplemods" && callback->id == "unbound" &&
            !FindAction("examplemods/invalid") && !FindAction("examplemods/badCallback") && !FindAction("Pause"),
            "valid native actions retain callback identity or a menu target, excluding invalid defaults");
        auto& input = OSFSettings::HotkeyInputState::Get();
        unsigned calls{};
        const auto fired = +[](const char*, const char*, void* context) noexcept { ++*static_cast<unsigned*>(context); };
        using OSFSettings::SettingsError;
        check(input.Register("examplemods", "badCallback", fired, &calls) == SettingsError::InvalidValue &&
            input.Register("examplemods", "openMenu", fired, &calls) == SettingsError::TypeMismatch &&
            input.Register("empty", "missing", fired, &calls) == SettingsError::UnknownHotkey,
            "registration publishes schema validation outcomes, including mods without hotkeys");
        check(input.Register("examplemods", "unbound", fired, &calls) == SettingsError::None,
            "valid unbound native actions accept callbacks");
        check(input.ProcessButton(0x75, *callback, 1, 0) && calls == 1,
            "production registration invokes the callback inline");


        load(&first, vanilla);
        check(parsedTexts.size() == 2 && parsedMap == &first && parsedTexts.back().ends_with(vanilla),
            "hook calls original parser once with the original map and intact vanilla suffix");
        const auto prefix = std::string_view(parsedTexts.back()).substr(0, parsedTexts.back().size() - std::strlen(vanilla));
        check(prefix.starts_with("examplemods/openMenu\t0x79*0\t") && prefix.ends_with('\n') &&
            std::ranges::count(prefix, '\n') == 4 && prefix.find("\n\n") == prefix.npos,
            "four complete rows join context zero without an extra context separator");

        load(&reset, vanilla);
        check(parsedTexts.size() == 3 && parsedMap == &reset && parsedTexts[1] == parsedTexts[2],
            "a vanilla reset receives the same definitions exactly once");
        const char pauseContexts[] = "Pause\t0x1b\t0xff\t0xff\t1\t0\t0\t0x8\t0\t1\r\n\r\n"
            "// Menu context\r\nPause\t0x70\t0xff\t0xff\t1\t0\t0\t0x8\t0\t1\r\n\r\n"
            "AnotherPause\t0x71\t0xff\t0xff\t1\t0\t0\t0x8\t0\t1";
        load(&reset, pauseContexts);
        check(parsedTexts.back() == OSFSettings::NativeHotkeys::g_hotkeyDefinitions + pauseContexts,
            "Pause contexts remain intact without adding action-specific links");
        fixture.Reset();
        load(&reset, vanilla);
        check(parsedTexts.size() == 5 && parsedTexts.back() == vanilla &&
            !OSFSettings::NativeHotkeys::g_parseHook,
            "fixture restores the original call and destroys the hook before host teardown");
        std::cout << checks << '/' << checks << " native registration checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
