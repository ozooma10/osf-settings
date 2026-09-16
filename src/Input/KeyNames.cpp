#include "KeyNames.h"
#include "Settings/SettingValue.h"
#include "SFSE/Impl/PCH.h"
#include "RE/B/BSInputDeviceManager.h"
#include "SFSE/InputMap.h"

#include <cstring>
#include <format>
#include <Windows.h>

namespace OSFSettings
{
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
        const auto keyCode = keyboard ? keyboard->GetKeyCodeFromName(std::string(name).c_str()) : SFSE::InputMap::GetKeyboardVirtualKey(name);
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
