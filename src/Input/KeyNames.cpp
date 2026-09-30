#include "KeyNames.h"
#include "Settings/Localization.h"
#include "Settings/SettingValue.h"
#include "SFSE/Impl/PCH.h"
#include "RE/B/BSInputDeviceManager.h"

#include <cstring>
#include <cwchar>
#include <format>
#include <Windows.h>

namespace OSFSettings
{
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
        if (name.size() == 6 && ::_strnicmp(name.data(), "MOUSE", 5) == 0 && name[5] >= '1' && name[5] <= '5') {
            return MouseVirtualKey(static_cast<std::uint32_t>(name[5] - '1'));
        }
        if (name.size() == 7 && ::_strnicmp(name.data(), "UNBOUND", 7) == 0) {
            return KeyBinding::Unbound;
        }
        if (name.empty() || name.find('\0') != std::string_view::npos) {
            return std::nullopt;
        }
        const auto* manager = RE::BSInputDeviceManager::GetSingleton();
        const auto* keyboard = manager ? manager->GetKeyboard() : nullptr;
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
        for (std::uint32_t button = 0; button < 5; ++button) {
            if (MouseVirtualKey(button) == keyCode) return "Mouse " + std::to_string(button + 1);
        }
        if (keyCode == KeyBinding::Unbound) return tr("values.unbound");
        if (keyCode > 0 && keyCode < KeyBinding::Unbound) {
            const auto* manager = RE::BSInputDeviceManager::GetSingleton();
            const auto* keyboard = manager ? manager->GetKeyboard() : nullptr;
            if (keyboard) {
                RE::BSFixedStringCS name;
                if (keyboard->GetKeyNameFromCode(keyCode, name) && !name.empty()) return name.c_str();
            }
        }
        return tr("keys.hex", {{"code", std::format("{:02X}", keyCode)}});
    }
}
