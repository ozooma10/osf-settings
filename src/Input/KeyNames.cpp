#include "KeyNames.h"
#include "Settings/SettingValue.h"
#include "SFSE/Impl/PCH.h"
#include "RE/B/BSInputDeviceManager.h"

#include <cstring>
#include <format>
#include <Windows.h>

namespace OSFSettings
{
    std::optional<std::uint32_t> KeyCodeFromTable(std::wstring_view table, std::string_view name)
    {
        if (name.empty() || name.find('\0') != std::string_view::npos) return std::nullopt;
        const auto lower = [](unsigned ch) { return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch; };
        for (std::size_t offset = 0; offset < table.size();) {
            const auto end = table.find(L'\n', offset);
            const auto row = table.substr(offset, end == table.npos ? table.size() - offset : end - offset);
            offset = end == table.npos ? table.size() : end + 1;
            const auto tab = row.find(L'\t');
            if (tab != name.size()) continue;
            bool matches = true;
            for (std::size_t i = 0; i < tab; ++i) matches &= lower(row[i]) == lower(static_cast<unsigned char>(name[i]));
            if (!matches) continue;
            auto code = row.substr(tab + 1);
            code = code.substr(0, code.find_first_of(L"\t\r"));
            if (code.starts_with(L"0x") || code.starts_with(L"0X")) code.remove_prefix(2);
            if (code.empty() || code.size() > 2) return std::nullopt;
            std::uint32_t value{};
            for (const auto ch : code) {
                const auto digit = lower(ch);
                if (digit >= '0' && digit <= '9') value = value * 16 + digit - '0';
                else if (digit >= 'a' && digit <= 'f') value = value * 16 + digit - 'a' + 10;
                else return std::nullopt;
            }
            return value;
        }
        return std::nullopt;
    }

    namespace
    {
        std::optional<std::uint32_t> EarlyKeyCode(std::string_view name)
        {
            if (!::GetModuleHandleW(L"Starfield.exe")) return std::nullopt;
            const auto& module = REX::FModule::GetExecutingModule();
            if (module.GetFileVersion() != REL::Version{1, 16, 244, 0}) return std::nullopt;
            const auto address = REL::Relocation<std::uintptr_t>{REL::ID(361050)}.address();
            if (address != module.GetBaseAddress() + 0x4A56010) return std::nullopt;
            // Verified native table: 115 names, 2778 bytes including UTF-16 NUL.
            // Check all bytes before parsing; there is no copied alias table/cache.
            constexpr std::size_t bytes = 2778;
            std::uint64_t hash = 14695981039346656037ull;
            const auto* data = reinterpret_cast<const unsigned char*>(address);
            for (std::size_t i = 0; i < bytes; ++i) hash = (hash ^ data[i]) * 1099511628211ull;
            if (hash != 0xEE1F5EE164E5555Full) return std::nullopt;
            return KeyCodeFromTable({reinterpret_cast<const wchar_t*>(address), bytes / 2 - 1}, name);
        }
    }

    std::optional<std::uint32_t> KeyCodeFromName(std::string_view name)
    {
        if (name.size() == 7 && ::_strnicmp(name.data(), "UNBOUND", 7) == 0) {
            return KeyBinding::Unbound;
        }
        if (name.empty() || name.find('\0') != std::string_view::npos) {
            return std::nullopt;
        }
        const auto* manager = RE::BSInputDeviceManager::GetSingleton();
        const auto* keyboard = manager ? manager->GetKeyboard() : nullptr;
        if (!keyboard) return EarlyKeyCode(name);
        const auto keyCode = keyboard->GetKeyCodeFromName(std::string(name).c_str());
        if (keyCode == 0xFFFFFFFF) return std::nullopt;
        return keyCode;
    }

    bool IsBindableKey(std::uint32_t keyCode)
    {
        if (keyCode == 0 || keyCode >= KeyBinding::Unbound) return false;
        switch (keyCode) {
        case VK_LBUTTON:
        case VK_RBUTTON:
        case VK_MBUTTON:
        case VK_XBUTTON1:
        case VK_XBUTTON2:
        case VK_ESCAPE:
            return false;
        default:
            return true;
        }
    }

    std::string KeyName(std::uint32_t keyCode)
    {
        if (keyCode == KeyBinding::Unbound) return "UNBOUND";
        if (keyCode > 0 && keyCode < KeyBinding::Unbound) {
            const auto* manager = RE::BSInputDeviceManager::GetSingleton();
            const auto* keyboard = manager ? manager->GetKeyboard() : nullptr;
            if (keyboard) {
                RE::BSFixedStringCS name;
                if (keyboard->GetKeyNameFromCode(keyCode, name) && !name.empty()) return name.c_str();
            }
        }
        return std::format("Key 0x{:02X}", keyCode);
    }
}
