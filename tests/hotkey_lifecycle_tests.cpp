#include "SFSE/Impl/PCH.h"
// Exercise the production startup hook and input callback against executable fixtures.
#include "../src/Input/HotkeyInput.cpp"

#include <iostream>
#include <cstring>
#include <stdexcept>

namespace
{
    using namespace OSFSettings;
    using Function = void (*)(RE::MenuControls*);
    std::byte* initializeCode{};
    RE::MenuControls* singleton{};
    RE::BSInputEventUser vanilla[7];
    RE::BSInputEventUser* handlers[8]{};
    std::uintptr_t nativeVtable[10]{};
    std::uintptr_t allocatorVtable[4]{};
    unsigned initializations{}, constructions{}, destructions{};
    bool attached{};
    std::vector<std::string_view> events;
    RE::UIMessageQueue queueStorage;
    RE::UIMessageQueue* queue = &queueStorage;
    std::vector<std::pair<std::string, RE::UI_MESSAGE_TYPE>> messages;

    class Event final : public RE::ButtonEvent
    {
    public:
        explicit Event(const char* name) { strUserEvent = name; deviceType = DeviceType::kKeyboard; eventType = EventType::kButton; }
        const RE::BSFixedString& QUserEvent() const override { return strUserEvent; }
    };

    void GetString(RE::BSStringPool::Entry*& result, const char* text, bool)
    {
        const auto length = std::strlen(text);
        auto* memory = new std::byte[sizeof(RE::BSStringPool::Entry) + length + 1]{};
        result = new (memory) RE::BSStringPool::Entry{};
        result->_length = static_cast<std::uint32_t>(length);
        result->_refCount = 1;
        std::memcpy(result + 1, text, length + 1);
    }

    void ReleaseString(RE::BSStringPool::Entry*& entry)
    {
        if (entry && --REX::TAtomicRef(entry->_refCount) == 0) delete[] reinterpret_cast<std::byte*>(entry);
        entry = nullptr;
    }

    void RecordMessage(RE::UIMessageQueue* receiver, const RE::BSFixedString& name, RE::UI_MESSAGE_TYPE type)
    {
        if (receiver != queue) throw std::runtime_error("Wrong message queue");
        messages.emplace_back(name.c_str(), type);
    }

    class Controls final : public RE::MenuControls
    {
    public:
        Controls() = default;
        ~Controls() override = default;
    };

    void OriginalInitialize(RE::MenuControls* controls)
    {
        ++initializations;
        events.push_back("initialize");
        for (unsigned i = 0; i < 7; ++i) handlers[i] = &vanilla[i];
        controls->handlers = { 7, 8, handlers };
    }

    RE::BSInputEventUser* Construct(RE::BSInputEventUser* user)
    {
        ++constructions;
        events.push_back("construct");
        const auto table = reinterpret_cast<std::uintptr_t>(nativeVtable);
        std::memcpy(user, &table, sizeof(table));
        user->pad08[0] = 0xA5;
        return user;
    }

    void* Destroy(RE::BSInputEventUser* user, std::uint32_t flags)
    {
        if (flags || user->pad08[0] != 0xA5 || attached)
            throw std::runtime_error("Invalid native held-state destruction");
        ++destructions;
        events.push_back("destroy");
        user->pad08[0] = 0;
        return user;
    }

    std::uint32_t Append(RE::MenuControls::HandlerStorage* array, const void*, std::uint32_t, std::uint32_t)
    {
        if (array->size >= array->capacity) throw std::runtime_error("Fixture capacity exceeded");
        events.push_back("register");
        return array->size++;
    }

    bool OriginalCall()
    {
        const auto address = reinterpret_cast<std::uintptr_t>(initializeCode + 0x303);
        return initializeCode[0x303] == std::byte{ 0xE8 } &&
            REL::ASM::CALL5::TARGET(address) == reinterpret_cast<std::uintptr_t>(&OriginalInitialize);
    }

    // Fixture-only reset: production keeps its single startup hook for the process.
    void ResetInitializeFixture()
    {
        if (HotkeyInput::g_initializeHook) HotkeyInput::g_initializeHook->Disable();
        ::FlushInstructionCache(::GetCurrentProcess(), nullptr, 0);
        HotkeyInput::g_initializeHook.reset();
    }

    struct HookFixture
    {
        ~HookFixture()
        {
            ResetInitializeFixture();
        }
    };
}

namespace OSFSettings::TestHarness
{

    void ObserveInput(const RE::ButtonEvent*, bool) noexcept {}
    void SetInputAttached(bool value) noexcept
    {
        attached = value;
        events.push_back(value ? "attach" : "invalidate");
    }
}

namespace OSFSettings::NativeHotkeys
{
    std::string_view GetMenu(std::string_view action)
    {
        if (action == "osfsettings/openMenu") return "OSFSettingsMenu";
        if (action == "anothermod/openMenu") return "OtherMenu";
        return {};
    }
}

// Substitute the engine address table and the code-write boundary.
// Writes still use VirtualProtect and real executable memory.
namespace REL
{
    IDDB::IDDB() = default;
    std::uint64_t IDDB::offset(std::uint64_t id) const
    {
        std::uintptr_t address{};
        switch (id) {
        case 99490: address = reinterpret_cast<std::uintptr_t>(initializeCode); break;
        case 114215: address = reinterpret_cast<std::uintptr_t>(&OriginalInitialize); break;
        case 938076: address = reinterpret_cast<std::uintptr_t>(&singleton); break;
        case 74686: address = reinterpret_cast<std::uintptr_t>(&Construct); break;
        case 74688: address = reinterpret_cast<std::uintptr_t>(&Destroy); break;
        case 123859: address = reinterpret_cast<std::uintptr_t>(&Append); break;
        case 392794: address = reinterpret_cast<std::uintptr_t>(allocatorVtable); break;
        case 937897: address = reinterpret_cast<std::uintptr_t>(&queue); break;
        case 130659: address = reinterpret_cast<std::uintptr_t>(&RecordMessage); break;
        case 1186742: address = reinterpret_cast<std::uintptr_t>(&GetString); break;
        case 139340: address = reinterpret_cast<std::uintptr_t>(&ReleaseString); break;
        default: throw std::runtime_error("Unexpected relocation: " + std::to_string(id));
        }
        return address - REX::FModule::GetExecutingModule().GetBaseAddress();
    }

    void Write(void* dst, const void* src, std::size_t size) { std::memcpy(dst, src, size); }
    void Write(std::uintptr_t dst, const void* src, std::size_t size) { Write(reinterpret_cast<void*>(dst), src, size); }
    bool WriteSafe(void* dst, const void* src, std::size_t size)
    {
        DWORD protect{};
        if (!::VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &protect)) return false;
        std::memcpy(dst, src, size);
        const bool restored = ::VirtualProtect(dst, size, protect, &protect);
        return restored;
    }
    bool WriteSafe(std::uintptr_t dst, const void* src, std::size_t size)
    {
        return WriteSafe(reinterpret_cast<void*>(dst), src, size);
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
        REL::Trampoline code;
        code.create(4096);
        initializeCode = static_cast<std::byte*>(code.allocate(0x320));
        std::memset(initializeCode, 0x90, 0x320);
        std::memcpy(initializeCode, "\x48\x83\xEC\x28", 4);
        REL::WriteData(initializeCode + 0x303, REL::ASM::CALL5{
            reinterpret_cast<std::uintptr_t>(initializeCode + 0x303), reinterpret_cast<std::uintptr_t>(&OriginalInitialize) });
        std::memcpy(initializeCode + 0x308, "\x48\x83\xC4\x28\xC3", 5);
        ::FlushInstructionCache(::GetCurrentProcess(), nullptr, 0);
        const auto initialize = reinterpret_cast<Function>(initializeCode);
        Controls controls;
        HookFixture fixture;

        REL::GetTrampoline().create(4096);
        initializeCode[0x303] = std::byte{ 0x90 };
        check(!HotkeyInput::Install() && !HotkeyInput::g_initializeHook,
            "reject a changed initialization opcode before patching");
        initializeCode[0x303] = std::byte{ 0xE8 };
        singleton = &controls;
        check(!HotkeyInput::Install() && OriginalCall(), "refuse installation after singleton creation");
        singleton = nullptr;

        controls.handlers = { 1, 0, nullptr };
        HotkeyInput::Attach(&controls);
        check(!HotkeyInput::g_handler && constructions == 1 && destructions == 1 && !attached &&
            events == std::vector<std::string_view>{ "construct", "destroy" },
            "failed registration destroys the handler and leaves the optional ready for retry");

        check(HotkeyInput::Install(), "install the single initialization hook");
        const auto allocated = REL::GetTrampoline().allocated_size();
        check(HotkeyInput::Install() && REL::GetTrampoline().allocated_size() == allocated,
            "repeated installation reuses the initialization hook");
        events.clear();
        initialize(&controls);
        check(initializations == 1 && constructions == 2 && attached && controls.GetHandlerCount() == 8 &&
            events == std::vector<std::string_view>{ "initialize", "construct", "register", "attach" },
            "vanilla initialization precedes exactly one native handler registration");
        {
            auto* handler = &*HotkeyInput::g_handler;
            Event settings("osfsettings/openMenu"), otherMenu("anothermod/openMenu"), pause("Pause");
            check(handler->ShouldHandleEvent(&settings) && handler->ShouldHandleEvent(&otherMenu) &&
                !handler->ShouldHandleEvent(&pause) && !handler->ShouldHandleEvent(nullptr),
                "our handler accepts declared menu actions without impersonating or accepting Pause");
            settings.disabled = true;
            check(!handler->ShouldHandleEvent(&settings), "disabled mapped actions are rejected");
            settings.disabled = false;
            settings.deviceType = RE::InputEvent::DeviceType::kGamepad;
            check(!handler->ShouldHandleEvent(&settings), "menu declarations remain keyboard-only");
            settings.deviceType = RE::InputEvent::DeviceType::kKeyboard;
            settings.eventType = RE::InputEvent::EventType::kChar;
            check(!handler->ShouldHandleEvent(&settings), "text events are not button activations");
            settings.eventType = RE::InputEvent::EventType::kButton;
            settings.value = 1;
            settings.heldDownSecs = 0;
            handler->OnButtonEvent(&settings);
            settings.heldDownSecs = 2;
            handler->OnButtonEvent(&settings);
            check(messages.empty() && settings.status == RE::InputEvent::Status::kUnhandled && handler->ShouldHandleEvent(&settings),
                "presses and repeats remain admitted for held tracking without opening or consuming");
            settings.value = 0;
            settings.heldDownSecs = -1;
            handler->OnButtonEvent(&settings);
            check(messages.empty(), "negative hold sentinel does not activate");
            settings.heldDownSecs = 0.1F;
            settings.next = &pause;
            handler->OnButtonEvent(&settings);
            check(messages == std::vector<std::pair<std::string, RE::UI_MESSAGE_TYPE>>{{"OSFSettingsMenu", RE::UI_MESSAGE_TYPE::kShow}} &&
                settings.status == RE::InputEvent::Status::kStop && settings.next == &pause &&
                std::string_view(settings.QUserEvent().c_str()) == "osfsettings/openMenu",
                "release queues Settings show and consumes without changing event identity or linkage");
            otherMenu.value = 0;
            otherMenu.heldDownSecs = 1;
            handler->OnButtonEvent(&otherMenu);
            check(messages.size() == 2 && messages.back().first == "OtherMenu" && messages.back().second == RE::UI_MESSAGE_TYPE::kShow,
                "another mod's menu opens through the identical callback");
            pause.value = 0;
            pause.heldDownSecs = 0;
            handler->OnButtonEvent(&pause);
            check(messages.size() == 2 && pause.status == RE::InputEvent::Status::kUnhandled, "unrecognized actions cannot enqueue or consume");
            queue = nullptr;
            settings.status = RE::InputEvent::Status::kUnhandled;
            handler->OnButtonEvent(&settings);
            check(messages.size() == 2 && settings.status == RE::InputEvent::Status::kUnhandled, "absent queue leaves the release unconsumed");
            queue = &queueStorage;
        }
        events.clear();
        HotkeyInput::Attach(&controls);
        check(constructions == 2 && destructions == 1 && attached && controls.GetHandlerCount() == 8 &&
            controls.handlers.data[7] == &*HotkeyInput::g_handler && events.empty(),
            "duplicate attachment retains the one process-lifetime handler without reconstruction");
        std::cout << checks << '/' << checks << " hotkey lifecycle checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
