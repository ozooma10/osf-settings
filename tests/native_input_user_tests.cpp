#include "SFSE/Impl/PCH.h"
#include "RE/B/BSInputEventUserStandalone.h"
#include "RE/M/MenuControls.h"

#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
    unsigned constructions{}, destructions{}, callbacks{};
    bool releasedWithoutFree{};
    std::uintptr_t nativeVtable[10]{};
    std::uintptr_t allocatorVtable[4]{};
    RE::BSInputEventUser* grownStorage[4]{};
    RE::MenuControls* singleton{};
    RE::MenuControls* removedFrom{};
    RE::BSInputEventUser* removedHandler{};
    RE::BSFixedString disabledName;
    unsigned appends{}, removals{}, nameReads{};
    bool failAppend{};

    std::uint32_t Append(RE::MenuControls::HandlerStorage* array, const void* allocator,
        std::uint32_t capacity, std::uint32_t elementSize)
    {
        ++appends;
        std::uintptr_t table{};
        RE::MenuControls::HandlerStorage* owner{};
        std::memcpy(&table, allocator, sizeof(table));
        std::memcpy(&owner, static_cast<const std::byte*>(allocator) + 8, sizeof(owner));
        if (table != reinterpret_cast<std::uintptr_t>(allocatorVtable) || owner != array ||
            capacity != array->capacity || elementSize != sizeof(RE::BSInputEventUser*)) {
            throw std::runtime_error("Incorrect native array/allocator call boundary");
        }
        if (failAppend) return 0xFFFFFFFFu;
        if (array->size == capacity) {
            std::copy_n(array->data, array->size, grownStorage);
            array->data = grownStorage;
            array->capacity = 4;
        }
        return array->size++;
    }

    void Remove(RE::MenuControls* controls, RE::BSInputEventUser* handler)
    {
        ++removals;
        removedFrom = controls;
        removedHandler = handler;
    }

    const RE::BSFixedString& QueryName(const RE::IDEvent* event)
    {
        ++nameReads;
        return event->disabled ? disabledName : event->strUserEvent;
    }

    void ReleaseString(RE::BSStringPool::Entry*& entry)
    {
        // These fixtures contain only empty strings; no engine pool is present.
        entry = nullptr;
    }

    RE::BSInputEventUser* Construct(RE::BSInputEventUser* user)
    {
        ++constructions;
        const auto table = reinterpret_cast<std::uintptr_t>(nativeVtable);
        std::memcpy(user, &table, sizeof(table));
        user->pad08[0] = 0xA5;
        user->inputEventHandlingEnabled = true;
        return user;
    }

    void* Destroy(RE::BSInputEventUser* user, std::uint32_t flags)
    {
        ++destructions;
        releasedWithoutFree = flags == 0 && user->pad08[0] == 0xA5;
        user->pad08[0] = 0;
        return user;
    }

    class Handler final : public RE::BSInputEventUserStandalone
    {
    public:
        bool ShouldHandleEvent(const RE::InputEvent*) override { return pad08[0] == 0xA5; }
        void OnButtonEvent(const RE::ButtonEvent*) override { ++callbacks; }
    };
    static_assert(!std::is_copy_constructible_v<Handler>);
    static_assert(!std::is_move_constructible_v<Handler>);
    static_assert(std::is_same_v<decltype(std::declval<const RE::IDEvent&>().QUserEvent()), const RE::BSFixedString&>);

    // A local fixture owns only its test storage; it does not run native MenuControls setup.
    class Controls final : public RE::MenuControls
    {
    public:
        Controls() = default;
        ~Controls() override = default;
    };
}

namespace REL
{
    IDDB::IDDB() = default;
    std::uint64_t IDDB::offset(std::uint64_t id) const
    {
        std::uintptr_t address{};
        switch (id) {
        case 74686: address = reinterpret_cast<std::uintptr_t>(&Construct); break;
        case 74688: address = reinterpret_cast<std::uintptr_t>(&Destroy); break;
        case 123859: address = reinterpret_cast<std::uintptr_t>(&Append); break;
        case 392794: address = reinterpret_cast<std::uintptr_t>(allocatorVtable); break;
        case 114217: address = reinterpret_cast<std::uintptr_t>(&Remove); break;
        case 938076: address = reinterpret_cast<std::uintptr_t>(&singleton); break;
        case 124035: address = reinterpret_cast<std::uintptr_t>(&QueryName); break;
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
        // Model the native constructor replacing the vptr. Calls through the
        // base must still reach the final derived handler after construction.
        alignas(Handler) std::byte storage[sizeof(Handler)];
        auto* handler = std::construct_at(reinterpret_cast<Handler*>(storage));
        RE::BSInputEventUser* user = handler;
        check(constructions == 1 && destructions == 0 && user->inputEventHandlingEnabled,
            "construct native held state exactly once");
        check(user->ShouldHandleEvent(nullptr), "restore the derived vtable and preserve native state");
        user->OnButtonEvent(nullptr);
        check(callbacks == 1, "dispatch through the final derived button callback");
        std::destroy_at(handler);
        check(destructions == 1 && releasedWithoutFree, "release native state without freeing OSF storage");

        handler = std::construct_at(reinterpret_cast<Handler*>(storage));
        std::destroy_at(handler);
        check(constructions == 2 && destructions == 2 && releasedWithoutFree,
            "reuse storage with balanced native construction and cleanup");

        Controls controls;
        singleton = &controls;
        RE::BSInputEventUser first, second, third;
        RE::BSInputEventUser* initialStorage[]{ &first };
        controls.handlers = { 1, 1, initialStorage };
        check(RE::MenuControls::GetSingleton() == &controls, "resolve the engine-owned singleton pointer");
        check(!controls.RegisterHandler(nullptr) && appends == 0, "reject null handlers before invoking native growth");
        second.inputEventHandlingEnabled = false;
        check(controls.RegisterHandler(&second) && appends == 1 && second.inputEventHandlingEnabled,
            "register through the native allocator boundary and enable the new handler");
        check(controls.GetHandlerCount() == 2 && controls.GetHandlers()[0] == &first &&
            controls.GetHandlers()[1] == &second && controls.handlers.data == grownStorage,
            "preserve existing entries and use the new backing storage returned by native growth");
        second.inputEventHandlingEnabled = false;
        check(controls.RegisterHandler(&second) && appends == 1 && controls.GetHandlerCount() == 2 &&
            !second.inputEventHandlingEnabled, "duplicate registration neither inserts nor re-enables a handler");
        failAppend = true;
        check(!controls.RegisterHandler(&third) && controls.GetHandlerCount() == 2 && !grownStorage[2],
            "native append failure leaves the list unchanged");
        controls.handlers.size = 5;
        check(!controls.RegisterHandler(&third) && appends == 2, "reject invalid array metadata before native growth");
        controls.handlers.size = 2;
        controls.UnregisterHandler(&second);
        check(removals == 1 && removedFrom == &controls && removedHandler == &second && destructions == 2,
            "unregister passes the exact receiver/handler to native removal without destroying borrowed objects");
        controls.UnregisterHandler(nullptr);
        check(removals == 1, "null removal does not invoke native code");

        RE::ButtonEvent button;
        const RE::InputEvent* event = &button;
        check(&event->QUserEvent() == &button.strUserEvent && nameReads == 1,
            "virtual name access preserves the borrowed reference returned by native code");
        button.disabled = true;
        check(&event->QUserEvent() == &disabledName && nameReads == 2,
            "disabled input preserves the native accessor's selected reference");
        std::cout << checks << '/' << checks << " native input checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
