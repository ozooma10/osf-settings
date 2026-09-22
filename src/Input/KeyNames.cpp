#include "KeyNames.h"
#include "Settings/Localization.h"
#include "Settings/SettingValue.h"
#include "SFSE/Impl/PCH.h"
#include "RE/B/BSInputDeviceManager.h"
#include "REX/W32/DINPUT.h"

#include <cstring>
#include <cwchar>
#include <format>
#include <Windows.h>

namespace OSFSettings
{
    std::uint32_t VirtualKeyToKeycode(const std::uint32_t virtualKey)
    {
        if (virtualKey == 0xFF || virtualKey == 0x7FFFFFFF) {
            return 0;
        }

        switch (virtualKey) {
        case VK_PAUSE:
            return REX::W32::DIK_PAUSE;
        case VK_NUMLOCK:
            return REX::W32::DIK_NUMLOCK;
        case VK_SNAPSHOT:
            return REX::W32::DIK_SYSRQ;
        default:
            break;
        }

        const auto scanCode = ::MapVirtualKeyExW(virtualKey, MAPVK_VK_TO_VSC_EX, ::GetKeyboardLayout(0));
        if (scanCode == 0) {
            return 0;
        }

        const auto prefix = (scanCode >> 8) & 0xFF;
        const auto set1 = scanCode & 0xFF;
        return prefix == 0xE0 || prefix == 0xE1 ? set1 | 0x80 : set1;
    }

    std::uint32_t GetKeyboardVirtualKey(std::string_view keyName)
    {
        std::wstring name;
        if (keyName.empty() || !REX::UTF8_TO_UTF16(keyName, name))
            return 0xFFFFFFFF;
        static REL::Relocation<const wchar_t*> table{ RE::ID::BSWin32KeyboardDevice::KeyNameTable };
        std::wstring_view rows{ table.get() };
        while (!rows.empty()) {
            const auto end = rows.find(L'\n');
            const auto row = rows.substr(0, end);
            const auto tab = row.find(L'\t');
            if (tab == name.size() && ::_wcsnicmp(row.data(), name.c_str(), tab) == 0) {
                return static_cast<std::uint32_t>(std::wcstoul(row.data() + tab + 1, nullptr, 16));
            }
            if (end == rows.npos)
                break;
            rows.remove_prefix(end + 1);
        }
        return 0xFFFFFFFF;
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
        REX::INFO("Attempting to get keyboard from input device manager {}", manager ? keyboard ? "succeeded" : "failed keyboard" : "failed manager");
        const auto keyCode = keyboard ? keyboard->GetKeyCodeFromName(std::string(name).c_str()) : GetKeyboardVirtualKey(name);
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
        if (keyCode == KeyBinding::Unbound) return Localization::Text("values.unbound");
        if (keyCode > 0 && keyCode < KeyBinding::Unbound) {
            const auto* manager = RE::BSInputDeviceManager::GetSingleton();
            const auto* keyboard = manager ? manager->GetKeyboard() : nullptr;
            if (keyboard) {
                RE::BSFixedStringCS name;
                if (keyboard->GetKeyNameFromCode(keyCode, name) && !name.empty()) return name.c_str();
            }
        }
        return Localization::Text("keys.hex", {{"code", std::format("{:02X}", keyCode)}});
    }
}
