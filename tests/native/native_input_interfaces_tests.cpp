#include "SFSE/Impl/PCH.h"
#include "RE/B/BSInputEventUser.h"
#include "RE/C/ControlMap.h"
#include "RE/C/ControlsRemappedEvent.h"
#include "REL/Trampoline.h"
#define NOMINMAX
#include <Windows.h>
#undef ERROR

#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace
{
    using Map = RE::ControlMap;
    using Device = RE::InputEvent::DeviceType;
    using Context = Map::InputContextID;
    using Slot = Map::BindingSlot;
    unsigned constructed{}, destroyed{}, dispatched{}, saved{}, notified{};
    std::uint32_t destructionFlags{ ~0u };
    const RE::InputEvent* receivedEvent{};
    const RE::ControlsRemappedEvent* receivedNotification{};
    RE::BSFixedString disabledName;
    std::int32_t eventInit{};
    RE::BSTEventSource<RE::ControlsRemappedEvent> eventSource;
    std::array<const char*, Map::INPUT_CONTEXT_NAME_COUNT> contextNames{};
    std::array<char, 260> directory{};
    const char* filename{};
    struct RemapCall
    {
        Map* map{};
        const RE::BSFixedStringCS* event{};
        std::uint8_t device{};
        const Map::KeyPair* keys{};
        Slot slot{};
        Context context{};
    } lastRemap;
    const char* parsedText{};
    std::string parsedDefinitions;
    unsigned parsed{};
    Map* calledMap{};

    RE::BSInputEventUser* Construct(RE::BSInputEventUser* user)
    {
        ++constructed;
        // Native construction overwrites the base vptr. Derived C++ construction
        // must restore its own table after this call; the test exercises that order.
        static RE::BSInputEventUser base;
        std::memcpy(user, &base, sizeof(void*));
        user->inputEventHandlingEnabled = true;
        return user;
    }
    void Destroy(RE::BSInputEventUser*, std::uint32_t flags) { ++destroyed; destructionFlags = flags; }
    void Handle(RE::BSInputEventUser* user, const RE::InputEvent* event)
    {
        ++dispatched;
        receivedEvent = event;
        if (user->ShouldHandleEvent(event)) user->OnButtonEvent(event->AsButtonEvent());
    }
    bool Accept(RE::BSInputEventUser*, const RE::InputEvent*) { return true; }
    const RE::BSFixedString& UserEvent(const RE::IDEvent* event)
    {
        return event->disabled ? disabledName : event->strUserEvent;
    }
    Map::RemapResult Validate(Map* map, const RE::BSFixedStringCS* event, std::uint8_t device,
        const Map::KeyPair* keys, Slot slot, Context context)
    {
        lastRemap = {map, event, device, keys, slot, context};
        return Map::RemapResult::kCannotSwap;
    }
    bool Remap(Map* map, const RE::BSFixedStringCS* event, std::uint8_t device,
        const Map::KeyPair* keys, Slot slot, Context context)
    {
        lastRemap = {map, event, device, keys, slot, context};
        return false;
    }
    void Parse(Map* map, const char* text) { calledMap = map; parsedText = text; parsedDefinitions = text; ++parsed; }
    std::uint16_t Serialize(Map* map, std::uint8_t device, char* buffer)
    {
        calledMap = map;
        buffer[0] = static_cast<char>(device);
        return 0x1234;
    }
    void Save(Map* map) { calledMap = map; ++saved; }
    void Notify(const RE::ControlsRemappedEvent* event) { receivedNotification = event; ++notified; }
    int DetourValue() { return 99; }

    class Handler final : public RE::BSInputEventUser
    {
    public:
        Handler() : BSInputEventUser(NativeState{}) {}
        ~Handler() override { DestroyNativeState(); }
        bool ShouldHandleEvent(const RE::InputEvent* event) override { return event->AsButtonEvent() != nullptr; }
        void OnButtonEvent(const RE::ButtonEvent*) override { ++buttons; }
        unsigned buttons{};
    };

    // Borrow a stack record as an engine array. Detach before BSTArray teardown;
    // the fixture never asks the engine allocator to own this memory.
    struct MappingView
    {
        RE::BSTArray<Map::UserEventMapping>& array;
        MappingView(RE::BSTArray<Map::UserEventMapping>& target, Map::UserEventMapping& mapping) : array(target)
        {
            struct Header { std::uint32_t size, capacity; Map::UserEventMapping* data; } header{1, 1, &mapping};
            static_assert(sizeof(header) == sizeof(array));
            std::memcpy(&array, &header, sizeof(header));
        }
        ~MappingView() { std::memset(&array, 0, sizeof(array)); }
    };
}

std::uintptr_t NativeInputTestAddress(std::uint64_t id)
{
    using namespace RE::ID;
    if (id == BSInputEventUser::ctor.id()) return reinterpret_cast<std::uintptr_t>(&Construct);
    if (id == BSInputEventUser::dtor.id()) return reinterpret_cast<std::uintptr_t>(&Destroy);
    if (id == BSInputEventUser::HandleEvent.id()) return reinterpret_cast<std::uintptr_t>(&Handle);
    if (id == BSInputEventUser::Unk09.id()) return reinterpret_cast<std::uintptr_t>(&Accept);
    if (id == IDEvent::QUserEvent.id()) return reinterpret_cast<std::uintptr_t>(&UserEvent);
    if (id == ControlMap::ValidateRemap.id()) return reinterpret_cast<std::uintptr_t>(&Validate);
    if (id == ControlMap::RemapButton.id()) return reinterpret_cast<std::uintptr_t>(&Remap);
    if (id == ControlMap::ParseMappings.id()) return reinterpret_cast<std::uintptr_t>(&Parse);
    if (id == ControlMap::SerializeMappings.id()) return reinterpret_cast<std::uintptr_t>(&Serialize);
    if (id == ControlMap::SaveMappings.id()) return reinterpret_cast<std::uintptr_t>(&Save);
    if (id == ControlMap::InputContextNameTable.id()) return reinterpret_cast<std::uintptr_t>(contextNames.data());
    if (id == ControlMap::CustomMappingDirectory.id()) return reinterpret_cast<std::uintptr_t>(directory.data());
    if (id == ControlMap::CustomMappingFilename.id()) return reinterpret_cast<std::uintptr_t>(&filename);
    if (id == ControlsRemappedEvent::EventSourceInitState.id()) return reinterpret_cast<std::uintptr_t>(&eventInit);
    if (id == ControlsRemappedEvent::EventSourceStorage.id()) return reinterpret_cast<std::uintptr_t>(&eventSource);
    if (id == ControlsRemappedEvent::Dispatch.id()) return reinterpret_cast<std::uintptr_t>(&Notify);
    return 0;
}

int TestNativeInputInterfaces()
{
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        ++checks;
        if (!passed) throw std::runtime_error(std::string("Native input interfaces: ") + message);
    };
    static_assert(std::is_same_v<decltype(std::declval<const RE::IDEvent&>().QUserEvent()), const RE::BSFixedString&>);
    static_assert(sizeof(Device) == 1 && sizeof(RE::InputEvent::EventType) == 1);
    static_assert(!std::is_copy_constructible_v<Handler> && !std::is_move_constructible_v<Handler>);
    RE::ButtonEvent button;
    button.deviceType = Device::kKeyboard;
    button.eventType = RE::InputEvent::EventType::kButton;
    std::memset(button.pad09, 0xCD, sizeof(button.pad09));
    std::memset(button.pad11, 0xEF, sizeof(button.pad11));
    check(button.deviceType == Device::kKeyboard && button.AsButtonEvent() == &button, "native byte tags ignore adjacent padding");
    check(&button.QUserEvent() == &button.strUserEvent, "named action returns the original string storage by reference");
    button.disabled = 4;
    check(&button.QUserEvent() == &disabledName, "non-boolean disabled reason preserves native reference result");
    button.disabled = 0;
    check(!button.IsDown() && !button.IsPressed() && !button.IsRepeating(), "release is not a press or repeat");
    button.value = 1;
    check(button.IsDown() && button.IsPressed() && !button.IsRepeating(), "down edge");
    button.heldDownSecs = 0.1f;
    check(button.IsDown() && !button.IsPressed() && button.IsRepeating(), "held repeat");
    {
        RE::BSInputEventUser engineOwned;
        check(constructed == 0, "default base leaves initialization to complete engine constructors");
        Handler handler;
        check(constructed == 1 && handler.inputEventHandlingEnabled, "standalone handler initializes native state once");
        handler.HandleEvent(&button);
        check(dispatched == 1 && receivedEvent == &button && handler.buttons == 1, "derived vtable restored after native base construction");
    }
    check(destroyed == 1 && destructionFlags == 0, "only standalone state destroyed, without freeing enclosing storage");

    Map map{};
    RE::BSFixedStringCS action("Test.Action");
    const Map::KeyPair keys{0x79, 0xA2};
    const auto result = map.ValidateRemap(action, Device::kGamepad, keys, Slot::kAlternate, Context::kShipHUD);
    check(result == Map::RemapResult::kCannotSwap && lastRemap.map == &map && lastRemap.event == &action &&
        lastRemap.device == 2 && lastRemap.keys == &keys && lastRemap.slot == Slot::kAlternate && lastRemap.context == Context::kShipHUD,
        "six-argument validation ABI preserves device, pair, slot, context and failure");
    check(!map.RemapButton(action, Device::kMouse, keys, Slot::kMain, Context::kVehicle) &&
        lastRemap.device == 1 && lastRemap.slot == Slot::kMain && lastRemap.context == Context::kVehicle, "remap does not hide native failure");
    const char defaults[] = "native defaults";
    map.ParseMappings(defaults);
    check(calledMap == &map && parsedText == defaults, "parser borrows complete definition text");
    const std::string prefix = "// MainGameplay\nJump\t0x20\t0xff\t0x18000\t1\t1\t1\t0x401\t0x84\t0\n";
    const std::string suffix = "Forward\t0x57\t0xff\t0xff\t1\t0\t0\t0x1\t0x8e\t0\n\nJump\t0xff\t0xff\t0xff\t0\t0\t0\t0x10\t0x0\t0\n";
    const auto vanilla = prefix + suffix;
    const std::array actions{ Map::KeyboardAction{"Test.First", 0x79, 0x401}, Map::KeyboardAction{"Test.Second", 0xFF, 0x401} };
    const std::string rows = "Test.First\t0x79\t0xff\t0xff\t1\t0\t0\t0x401\t0x0\t0\n"
                             "Test.Second\t0xff\t0xff\t0xff\t1\t0\t0\t0x401\t0x0\t0\n";
    const auto beforeParse = parsed;
    check(map.ParseMappings(vanilla.c_str(), actions, Context::kMainGameplay, "Jump") && parsed == beforeParse + 1,
        "all declarations reach the native parser in one call");
    check(parsedDefinitions == prefix + rows + suffix,
        "bound/unbound defaults, gameplay mask, declared order and every vanilla byte are preserved");
    check(map.ParseMappings(vanilla.c_str(), actions, Context::kMainGameplay, "Jump") && parsedDefinitions == prefix + rows + suffix,
        "reset starts from vanilla definitions without accumulating actions");
    std::string crlf = vanilla, crlfExpected = prefix + rows + suffix;
    for (auto* text : { &crlf, &crlfExpected }) {
        for (std::size_t i = 0; i < text->size(); ++i) if ((*text)[i] == '\n') text->insert(i++, 1, '\r');
    }
    check(map.ParseMappings(crlf.c_str(), actions, Context::kMainGameplay, "Jump") && parsedDefinitions == crlfExpected,
        "inserted definitions follow the anchor's CRLF ending");
    check(map.ParseMappings(vanilla.c_str(), actions, Context::kConsoleOpening, "Jump") && parsedDefinitions == vanilla + rows,
        "the caller selects the context even when an earlier context has the same event name");
    const auto beforeMissing = parsed;
    check(!map.ParseMappings(vanilla.c_str(), actions, Context::kMainGameplay, "Missing") && parsed == beforeMissing,
        "missing anchor leaves native parsing and mappings untouched");
    for (const auto separator : { "\n", " \n", "\t\n" }) {
        const auto shifted = std::string(separator) + vanilla;
        check(!map.ParseMappings(shifted.c_str(), actions, Context::kMainGameplay, "Jump") && parsed == beforeMissing,
            "native context separators cannot insert an action in the wrong context");
    }
    check(!map.ParseMappings("// Jump\tcomment\nNotJump\tdata\nJumpExtra\tdata\n", actions, Context::kMainGameplay, "Jump"),
        "anchor matching uses complete event names and ignores comments");
    check(!map.ParseMappings("Jump\tunterminated", actions, Context::kMainGameplay, "Jump"),
        "an unterminated anchor is not an insertion point");
    check(map.ParseMappings(defaults, {}, Context::kMainGameplay, "Missing") && parsedText == defaults,
        "empty declarations forward the original defaults directly");
    std::array<char, Map::SERIALIZATION_BUFFER_SIZE> buffer{};
    check(map.SerializeMappings(Device::kGamepad, buffer) == 0x1234 && buffer[0] == 2, "serializer preserves device and 16-bit length");
    map.SaveMappings();
    check(saved == 1 && calledMap == &map, "native writer receives the map");
    check(Map::GetCustomMappingPath().empty(), "missing native path is rejected");
    strcpy_s(directory.data(), directory.size(), "D:\\Redirected\\");
    filename = "ControlMap_Custom.txt";
    check(Map::GetCustomMappingPath() == "D:\\Redirected\\ControlMap_Custom.txt", "path comes from native globals");
    directory.fill('x');
    check(Map::GetCustomMappingPath().empty(), "unterminated directory is rejected");
    directory.fill(0);

    check(map.CanSaveMappings() && map.CanSaveMappings(16381) && !map.CanSaveMappings(16382) &&
        !map.CanSaveMappings(std::numeric_limits<std::size_t>::max()), "empty map and reserve overflow boundaries");
    contextNames[0] = "MainGameplay";
    Map::InputContext context{};
    map.inputContexts[0] = &context;
    Map::UserEventMapping mapping{};
    MappingView view(context.deviceMappings[0], mapping);
    mapping.visibleInControls = true;
    mapping.unk1F = 1;
    mapping.eventID = std::string(16355, 'a');
    check(map.CanSaveMappings() && !map.CanSaveMappings(1), "final record at native buffer boundary");
    mapping.eventID = std::string(16356, 'a');
    check(!map.CanSaveMappings(), "oversized final record rejected before native write");
    mapping.unk1F = 0;
    check(map.CanSaveMappings(), "unchanged record omitted from custom file");
    mapping.unk1F = 1;
    mapping.visibleInControls = false;
    check(map.CanSaveMappings(), "hidden record omitted from custom file");

    check(RE::ControlsRemappedEvent::GetEventSource() == nullptr && notified == 0, "lookup does not synthesize an event");
    eventInit = -1;
    check(RE::ControlsRemappedEvent::GetEventSource() == nullptr, "lookup rejects partially initialized source");
    eventInit = std::numeric_limits<std::int32_t>::min() + 1;
    check(RE::ControlsRemappedEvent::GetEventSource() == &eventSource, "published source becomes available");
    const RE::ControlsRemappedEvent notification{1, 1};
    RE::ControlsRemappedEvent::Dispatch(notification);
    check(notified == 1 && receivedNotification == &notification, "notification forwards original payload storage");

    // Execute both entry detour sizes and their original gateways using a tiny
    // complete mov-eax/ret function. This tests instruction copying and return flow.
    auto* code = static_cast<std::byte*>(::VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!code) throw std::runtime_error("Could not allocate detour test code");
    struct FreeCode { void* code; ~FreeCode() { ::VirtualFree(code, 0, MEM_RELEASE); } } freeCode{ code };
    REL::Trampoline trampoline{ "native-interface-test" };
    trampoline.set_trampoline(code + 0x100, 0x200);
    const auto detour = [&]<std::size_t Size>(std::byte* source) {
        const std::uint8_t body[]{ 0xB8, 42, 0, 0, 0, 0xC3 }; // mov eax, 42; ret
        if constexpr (Size == 6) *source = std::byte{0x90}; // Complete nop + mov.
        std::memcpy(source + (Size - 5), body, sizeof(body));
        int (*original)(){};
        trampoline.write_detour<Size>(reinterpret_cast<std::uintptr_t>(source), &DetourValue, original);
        ::FlushInstructionCache(::GetCurrentProcess(), code, 0x1000);
        check(original && original() == 42 && reinterpret_cast<int (*)()>(source)() == 99,
            "entry detour redirects execution and retains a working original gateway");
    };
    detour.operator()<5>(code);
    detour.operator()<6>(code + 0x20);
    return checks;
}
