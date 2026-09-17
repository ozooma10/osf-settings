#include "SFSE/Impl/PCH.h"
// Include the implementation so this standalone fixture can remove its hook
// before destroying the synthetic loader and CommonLib test-process state.
#include "../src/Input/NativeHotkeys.cpp"
#include "Settings/SettingsService.h"
#include "RE/C/ControlMap.h"
#include "SFSE/InputMap.h"
#include "REL/ASM.h"
#include "REL/Trampoline.h"

#include <iostream>
#include <stdexcept>

namespace
{
    std::vector<OSFSettings::ModSettings> schemas;
    constexpr wchar_t keyTable[] = L"F4\t0x73\t115\nF10\t0x79\t121\nL Ctrl\t0xA2\t17\n/\t0xBF";
    std::byte* loader{};
    RE::ControlMap* parsedMap{};
    std::vector<std::string> parsedTexts;
    struct Row
    {
        std::string event;
        std::uint32_t key;
        bool nativeFlags;
    };
    std::vector<Row> formattedRows;

    struct HookFixture
    {
        void Reset()
        {
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
}

// Supply schema data and engine boundaries; execute the real lookup and hook.
namespace OSFSettings
{
    SettingsService& SettingsService::Get() { static SettingsService service; return service; }
    std::vector<ModSettings> SettingsService::Snapshot() const { return schemas; }
}

namespace RE
{
    std::string ControlMap::FormatMappingRow(const char* event, std::uint32_t keyboard,
        std::uint32_t mouse, std::uint32_t gamepad, bool keyboardVisible, bool mouseVisible, bool gamepadVisible,
        std::uint32_t controlMask, std::uint32_t groupMask, bool required)
    {
        formattedRows.push_back({ event, keyboard, mouse == 0xFF && gamepad == 0xFF && keyboardVisible &&
            !mouseVisible && !gamepadVisible && controlMask == (std::string_view(event).ends_with("/openMenu") ? 0x08u : 0x401u) && groupMask == 0 && !required });
        return std::format("{}\t{:#x}\t0xff\t0xff\t1\t0\t0\t{:#x}\t0\t0\n", event, keyboard, controlMask);
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
        HookFixture fixture;

        check(SFSE::InputMap::GetKeyboardVirtualKey("f10") == 0x79 &&
            SFSE::InputMap::GetKeyboardVirtualKey("L Ctrl") == 0xA2 &&
            SFSE::InputMap::GetKeyboardVirtualKey("/") == 0xBF,
            "native table lookup handles case, spaced names and a final row without newline");
        check(SFSE::InputMap::GetKeyboardVirtualKey("") == 0xFFFFFFFF &&
            SFSE::InputMap::GetKeyboardVirtualKey("Unknown") == 0xFFFFFFFF,
            "unknown names cannot silently become key zero");

        RE::ControlMap first{}, reset{};
        const char vanilla[] = "// MainGameplay\r\nJump\t0x20\t0xff\t0xff\t1\t0\t0\t0x401\t0\t0\r\n\r\n"
            "// ConsoleOpening\r\nConsole\t0xc0\t0xff\t0xff\t0\t0\t0\t0\t0\t0\r\n";
        check(OSFSettings::NativeHotkeys::Install(), "empty declarations need no hook");
        load(&first, vanilla);
        check(parsedTexts.size() == 1 && parsedTexts.back() == vanilla, "empty declarations preserve vanilla input");

        schemas.resize(2);
        schemas[0].schema.id = "osfsettings";
        schemas[0].schema.hotkeys = { { "openMenu", "Open settings", "F10", "OSFSettingsMenu" },
            { "unbound", "Unbound", std::nullopt }, { "invalid", "Invalid", "Unknown", "InvalidMenu" } };
        schemas[1].schema.id = "anothermod";
        schemas[1].schema.hotkeys = { { "openMenu", "Other action", "F4", "OtherMenu" } };
        check(OSFSettings::NativeHotkeys::Install(), "real CALL5 hook installs");
        check(formattedRows.size() == 3 && formattedRows[0].event == "osfsettings/openMenu" &&
            formattedRows[0].key == 0x79 && formattedRows[1].key == 0xFF &&
            formattedRows[2].event == "anothermod/openMenu" && formattedRows[2].key == 0x73,
            "schema defaults, unbound actions, namespaces and invalid defaults are handled");
        check(std::ranges::all_of(formattedRows, [](const Row& row) { return row.nativeFlags; }),
            "menu targets use the menu mask; mapping-only actions retain their gameplay flags");
        check(OSFSettings::NativeHotkeys::GetMenu("osfsettings/openMenu") == "OSFSettingsMenu" &&
            OSFSettings::NativeHotkeys::GetMenu("anothermod/openMenu") == "OtherMenu" &&
            OSFSettings::NativeHotkeys::GetMenu("osfsettings/unbound").empty() &&
            OSFSettings::NativeHotkeys::GetMenu("osfsettings/invalid").empty() &&
            OSFSettings::NativeHotkeys::GetMenu("Pause").empty(), "only valid declarations with a menu target are dispatched");

        load(&first, vanilla);
        check(parsedTexts.size() == 2 && parsedMap == &first && parsedTexts.back().ends_with(vanilla),
            "hook calls original parser once with the original map and intact vanilla suffix");
        const auto prefix = std::string_view(parsedTexts.back()).substr(0, parsedTexts.back().size() - std::strlen(vanilla));
        check(prefix.starts_with("osfsettings/openMenu\t0x79\t") && prefix.ends_with('\n') &&
            std::ranges::count(prefix, '\n') == 3 && prefix.find("\n\n") == prefix.npos,
            "three complete rows join context zero without an extra context separator");

        load(&reset, vanilla);
        check(parsedTexts.size() == 3 && parsedMap == &reset && parsedTexts[1] == parsedTexts[2],
            "a vanilla reset receives the same definitions exactly once");
        check(OSFSettings::NativeHotkeys::Install() && formattedRows.size() == 3,
            "repeated installation neither rebuilds rows nor chains the hook twice");
        load(&reset, vanilla);
        check(parsedTexts.size() == 4 && parsedTexts[2] == parsedTexts[3], "repeated installation preserves one parser call");
        const char pauseContexts[] = "Pause\t0x1b\t0xff\t0xff\t1\t0\t0\t0x8\t0\t1\r\n\r\n"
            "// Menu context\r\nPause\t0x70\t0xff\t0xff\t1\t0\t0\t0x8\t0\t1\r\n\r\n"
            "AnotherPause\t0x71\t0xff\t0xff\t1\t0\t0\t0x8\t0\t1";
        load(&reset, pauseContexts);
        check(parsedTexts.back() == OSFSettings::NativeHotkeys::g_hotkeyDefinitions + pauseContexts,
            "Pause contexts remain intact without adding action-specific links");
        fixture.Reset();
        load(&reset, vanilla);
        check(parsedTexts.size() == 6 && parsedTexts.back() == vanilla &&
            !OSFSettings::NativeHotkeys::g_parseHook,
            "fixture restores the original call and destroys the hook before host teardown");
        std::cout << checks << '/' << checks << " native registration checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
