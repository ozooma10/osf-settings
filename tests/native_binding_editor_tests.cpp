#include "SFSE/Impl/PCH.h"
#include "../src/Input/NativeBindingEditor.cpp"

#include <iostream>
#include <map>
#include <stdexcept>

namespace
{
    using namespace OSFSettings;
    using Validate = std::uint8_t (*)(RE::ControlMap*, const RE::BSFixedString*,
        std::uint8_t, const std::uint32_t*, Slot, Context);
    std::byte* evaluator{};
    alignas(RE::SettingsDataModel) std::byte modelStorage[sizeof(RE::SettingsDataModel)]{};
    auto* model = reinterpret_cast<RE::SettingsDataModel*>(modelStorage);
    std::uint8_t nativeResult{};
    unsigned validations{}, cancellations{};
    Slot receivedSlot{};
    Context receivedContext{};
    std::map<std::string, RE::BSStringPool::Entry*> strings;

    std::uint8_t ValidateNative(RE::ControlMap*, const RE::BSFixedString*, std::uint8_t,
        const std::uint32_t*, Slot slot, Context context)
    {
        ++validations;
        receivedSlot = slot;
        receivedContext = context;
        return nativeResult;
    }

    void CancelNative() { ++cancellations; }

    void GetString(RE::BSStringPool::Entry*& result, const char* text, bool)
    {
        auto& entry = strings[text];
        if (!entry) {
            const auto length = std::strlen(text);
            auto* memory = new std::byte[sizeof(RE::BSStringPool::Entry) + length + 1]{};
            entry = new (memory) RE::BSStringPool::Entry{};
            entry->_length = static_cast<std::uint32_t>(length);
            std::memcpy(entry + 1, text, length + 1);
        }
        ++entry->_refCount;
        result = entry;
    }

    void ReleaseString(RE::BSStringPool::Entry*& entry)
    {
        if (entry && --entry->_refCount == 0) {
            strings.erase(entry->data<char>());
            delete[] reinterpret_cast<std::byte*>(entry);
        }
        entry = nullptr;
    }

    struct Fixture
    {
        NativeBindingEditor editor;
        ~Fixture()
        {
            editor.End(false);
            if (g_validateHook) {
                g_validateHook->Disable();
                g_validateHook.reset();
            }
        }
    };
}

namespace REL
{
    IDDB::IDDB() = default;
    std::uint64_t IDDB::offset(std::uint64_t id) const
    {
        std::uintptr_t address{};
        switch (id) {
        case 88672: address = reinterpret_cast<std::uintptr_t>(evaluator); break;
        case 124132: address = reinterpret_cast<std::uintptr_t>(&ValidateNative); break;
        case 939451: address = reinterpret_cast<std::uintptr_t>(&model); break;
        case 88684: address = reinterpret_cast<std::uintptr_t>(&CancelNative); break;
        case 1186742: address = reinterpret_cast<std::uintptr_t>(&GetString); break;
        case 139340: address = reinterpret_cast<std::uintptr_t>(&ReleaseString); break;
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
        auto& trampoline = REL::GetTrampoline();
        trampoline.create(4096);
        evaluator = static_cast<std::byte*>(trampoline.allocate(0x200));
        std::memset(evaluator, 0x90, 0x200);
        // Six-argument Windows x64 call: forward the two byte-sized stack arguments.
        const unsigned char prologue[]{ 0x48, 0x83, 0xEC, 0x38,
            0x8A, 0x44, 0x24, 0x60, 0x88, 0x44, 0x24, 0x20,
            0x8A, 0x44, 0x24, 0x68, 0x88, 0x44, 0x24, 0x28 };
        std::memcpy(evaluator, prologue, sizeof(prologue));
        std::memcpy(evaluator + 0x18A, "\x48\x83\xC4\x38\xC3", 5);
        const auto evaluate = reinterpret_cast<Validate>(evaluator);
        Fixture fixture;
        check(!fixture.editor.Begin(), "capture unavailable before validation hook installation");
        check(!NativeBindingEditor::Install(), "reject unexpected validation opcode");
        // Install chains the existing call through THook; it does not validate a target ID.
        REL::WriteData(evaluator + 0x185, REL::ASM::CALL5{
            reinterpret_cast<std::uintptr_t>(evaluator + 0x185), reinterpret_cast<std::uintptr_t>(&ValidateNative) });
        check(NativeBindingEditor::Install() && NativeBindingEditor::Install(), "install once and reuse the hook");

        RE::ControlMap map{};
        RE::BSFixedString action("test/action");
        std::array<RE::ControlMap::UserEventMapping, 2> owners{};
        owners[0].eventID = "Jump";
        owners[0].keyCode = 0x20;
        owners[0].modifierKeyCode = 0xFF;
        owners[0].bindingSlot = Slot::kMain;
        owners[0].visibleInControls = true;
        owners[1].eventID = "test/action";
        owners[1].keyCode = 0x79;
        owners[1].modifierKeyCode = 0xFF;
        owners[1].visibleInControls = true;
        // Borrowed native array views, backed by fixture-owned mapping records.
        struct ArrayView { std::uint32_t size, capacity; RE::ControlMap::UserEventMapping* data; };
        std::array<ArrayView, 3> arrays{ ArrayView{ 2, 2, owners.data() }, ArrayView{ 2, 2, owners.data() }, ArrayView{ 2, 2, owners.data() } };
        static_assert(sizeof(arrays) == sizeof(RE::ControlMap::InputContext));
        map.inputContexts[0] = reinterpret_cast<RE::ControlMap::InputContext*>(arrays.data());
        std::uint32_t keys[]{ 0x20, 0xFF };
        const auto candidate = [&](Slot slot = Slot::kMain, Context context = Context::kMainGameplay,
                                   Device device = Device::kKeyboard) {
            const auto before = validations;
            const auto result = evaluate(&map, &action, static_cast<std::uint8_t>(device), keys, slot, context);
            if (validations != before + 1 || receivedSlot != slot || receivedContext != context)
                throw std::runtime_error("Original validator call/stack arguments were not preserved");
            return result;
        };
        check(candidate() == 0, "ordinary vanilla editing retains its allowed-swap policy");
        auto& hotkeys = OSFSettings::HotkeyInputState::Get();
        const NativeHotkeys::Action callbackAction{ "test/callback", "test", "callback", std::nullopt };
        const NativeHotkeys::Action menuAction{ "test/action", "test", "action", "TestMenu" };
        hotkeys.Initialize({ { "test", {{ "action", HotkeyInputState::Target::Menu }, { "callback", HotkeyInputState::Target::Callback }} } });
        unsigned callbacks{};
        check(hotkeys.Register("test", "callback", +[](const char*, const char*, void* context) noexcept {
            ++*static_cast<unsigned*>(context);
        }, &callbacks) == SettingsError::None, "register capture callback fixture");
        hotkeys.ProcessButton(0x75, callbackAction, 1, 0);
        hotkeys.ProcessButton(0x79, menuAction, 1, 0);
        check(fixture.editor.Begin(), "begin OSF capture");

        check(callbacks == 1 && !hotkeys.ProcessButton(0x75, callbackAction, 1, 0),
            "native capture blocks new callback presses after earlier inline delivery");
        check(candidate() == 2, "populated primary requires confirmation for another action's key");
        check(candidate(Slot::kAlternate) == 2, "secondary requires the same confirmation");
        owners[0].bindingSlot = Slot::kAlternate;
        check(candidate() == 2, "taking another action's alternate also requires confirmation");
        check(candidate(Slot::kMain, Context::kMainGameplay, Device::kMouse) == 2, "mouse binding conflicts also prompt");
        check(candidate(Slot::kMain, Context::kShipHUD) == 0, "unrelated contexts retain native behavior");
        check(candidate(Slot::kMain, Context::kMainGameplay, Device::kGamepad) == 2, "controller occupied bindings require confirmation during OSF capture");
        for (const auto result : { 1, 2, 3 }) {
            nativeResult = static_cast<std::uint8_t>(result);
            check(candidate() == result, "preserve native rejection or confirmation result");
        }
        nativeResult = 0;
        keys[0] = 0x79;
        check(candidate() == 0, "the same action's existing binding is not a conflict");
        keys[0] = 0x78;
        check(candidate() == 0, "unused keys still apply directly");
        keys[0] = 0xFF;
        check(candidate() == 0, "unbound candidate never creates a conflict");
        keys[0] = 0x20;
        keys[1] = 0x10;
        check(candidate() == 0, "match the complete key pair");
        keys[1] = 0xFF;
        fixture.editor.End(true);
        check(cancellations == 1 && !NativeBindingEditor::IsActive() && candidate() == 0,
            "cancel delegates once and releases OSF's policy");
        check(!hotkeys.ProcessButton(0x79, menuAction, 0, 1),
            "ending capture does not revive the key held before capture began");
        check(!hotkeys.ProcessButton(0x75, callbackAction, 1, 1) &&
            !hotkeys.ProcessButton(0x75, callbackAction, 0, 1), "capture cancellation cannot replay held callback input");
        const auto externalBlock = hotkeys.AcquireBlock();
        check(fixture.editor.Begin() && candidate() == 2, "cancel then retry the same occupied key prompts again");
        fixture.editor.End(false);
        check(cancellations == 1 && candidate() == 0, "completion releases ownership without cancelling");
        hotkeys.ProcessButton(0x79, menuAction, 1, 0);
        check(!hotkeys.ProcessButton(0x79, menuAction, 0, 1),
            "ending native capture preserves an external consumer's block");
        check(!hotkeys.ProcessButton(0x75, callbackAction, 1, 0), "external blocks still suppress callbacks after capture ends");
        hotkeys.ReleaseBlock(externalBlock);
        hotkeys.ProcessButton(0x79, menuAction, 1, 0);
        check(hotkeys.ProcessButton(0x79, menuAction, 0, 1),
            "new presses work after native and external owners release");
        check(hotkeys.ProcessButton(0x75, callbackAction, 1, 0), "fresh callback presses work after all owners release");

        check(callbacks == 2, "capture cleanup restores callback delivery");
        check(owners[0].keyCode == 0x20 && owners[1].keyCode == 0x79,
            "confirmation policy never mutates bindings before the native decision");
        std::cout << checks << " native binding editor checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
