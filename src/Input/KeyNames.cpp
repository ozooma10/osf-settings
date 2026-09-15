#include "KeyNames.h"
#include "Settings/SettingValue.h"
#include "SFSE/Impl/PCH.h"
#include "RE/B/BSInputDeviceManager.h"
#include "REX/CONVERT.h"

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
        if (!keyboard) return std::nullopt;
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
            // Convert only for Windows' display-name API; the binding remains a VK.
            auto scan = ::MapVirtualKeyExW(keyCode, MAPVK_VK_TO_VSC_EX, ::GetKeyboardLayout(0));
            if (keyCode == VK_PAUSE) {
                scan = 0x45;
            }
            if (scan) {
                auto parameter = static_cast<LONG>((scan & 0xFF) << 16);
                if ((scan >> 8) == 0xE0 || keyCode == VK_NUMLOCK) parameter |= 1 << 24;
                wchar_t buffer[256]{};
                const auto length = ::GetKeyNameTextW(parameter, buffer, 256);
                std::string name;
                if (length > 0 && REX::UTF16_TO_UTF8(std::wstring_view(buffer, length), name) && !name.empty()) return name;
            }
        }
        return std::format("Key 0x{:02X}", keyCode);
    }
}
