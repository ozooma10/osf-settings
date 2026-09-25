#include "Input/HotkeyInputState.h"
#include "HotkeyTasks.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    using namespace OSFSettings;
    using Target = HotkeyInputState::Target;
    constexpr std::uint32_t key = 0x75;
    const NativeHotkeys::Action action{ "sample/toggleFeature", "sample", "toggleFeature", std::nullopt };
    const NativeHotkeys::Action second{ "sample/second", "sample", "second", std::nullopt };
    const NativeHotkeys::Action menu{ "sample/openMenu", "sample", "openMenu", "SampleMenu" };

    struct Receiver
    {
        std::vector<std::string> events;
        std::function<void()> onCall;

        static void Fired(const char* mod, const char* id, void* context) noexcept
        {
            auto& self = *static_cast<Receiver*>(context);
            self.events.push_back(std::string(mod) + "/" + id);
            if (self.onCall) self.onCall();
        }
    };
}

int main()
{
    unsigned checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    try {
        HotkeyInputState input;
        Receiver one, two, another;
        const auto registerCallback = [&](const char* mod, const char* id, Receiver& receiver) {
            return input.Register(mod, id, Receiver::Fired, &receiver);
        };
        const auto press = [&] { return input.ProcessButton(key, action, 1, 0); };
        const auto repeat = [&] { return input.ProcessButton(key, action, 1, 1); };
        const auto release = [&] { return input.ProcessButton(key, action, 0, 1); };
        check(registerCallback("sample", "toggleFeature", one) == SettingsError::NotReady,
            "registration waits for native initialization");
        input.Initialize({});
        check(registerCallback("sample", "toggleFeature", one) == SettingsError::UnknownMod,
            "an initialized empty registry is ready but has no declared actions");
        input.Initialize({
            { "sample", {{ "toggleFeature", Target::Callback }, { "second", Target::Callback },
                { "openMenu", Target::Menu }, { "invalid", Target::Invalid }} },
            { "empty", {} }
        });
        check(registerCallback("missing", "toggleFeature", one) == SettingsError::UnknownMod &&
            registerCallback("empty", "toggleFeature", one) == SettingsError::UnknownHotkey &&
            registerCallback("sample", "ToggleFeature", one) == SettingsError::UnknownHotkey &&
            registerCallback("sample", "openMenu", one) == SettingsError::TypeMismatch &&
            registerCallback("sample", "invalid", one) == SettingsError::InvalidValue,
            "registration distinguishes missing mods, exact IDs, menu targets and invalid defaults");
        check(input.Register("sample", "toggleFeature", nullptr, nullptr) == SettingsError::InvalidArgument,
            "missing callbacks cannot be registered");
        check(!press() && !repeat() && !release() && HotkeyTasks::pending.empty(),
            "actions without callbacks do not consume input or submit tasks");

        std::string mod = "sample", id = "toggleFeature";
        check(input.Register(mod, id, Receiver::Fired, &one) == SettingsError::None, "register a callback");
        mod.clear(); id.clear();
        check(HotkeyTasks::pending.empty(), "registration owns its IDs and produces no initial notification");
        check(press() && HotkeyTasks::pending.size() == 1 && one.events.empty(),
            "a press submits one task without invoking inline");
        check(registerCallback("sample", "toggleFeature", two) == SettingsError::None, "register another callback");
        HotkeyTasks::Run();
        check(one.events == std::vector<std::string>{action.event} && two.events.empty(),
            "a task owns the callbacks present at its press, excluding later registrations");

        std::vector<int> order;
        one.onCall = [&] { order.push_back(1); };
        two.onCall = [&] { order.push_back(2); };
        check(press() && !repeat() && !release() && press() && HotkeyTasks::pending.size() == 2,
            "each fresh down submits one task for all callbacks, ignoring repeats and releases");
        HotkeyTasks::Run();
        check(order == std::vector<int>{1, 2, 1, 2}, "each task invokes its callbacks in registration order");
        one.onCall = {}; two.onCall = {};
        check(!input.ProcessButton(0, action, 1, 0) && !input.ProcessButton(255, action, 1, 0) &&
            !input.ProcessButton(0xFFFFFFFFu, action, 1, 0) &&
            !input.ProcessButton(key, action, 0, -1) &&
            !input.ProcessButton(key, action, std::numeric_limits<float>::quiet_NaN(), 0),
            "invalid keys and non-press callback values never activate");
        check(registerCallback("sample", "second", another) == SettingsError::None &&
            input.ProcessButton(key + 1, second, 1, 0), "register a different action");
        HotkeyTasks::Run();
        check(another.events == std::vector<std::string>{"sample/second"} && one.events.size() == 3,
            "hotkey identity isolates delivery");
        check(!input.ProcessButton(key + 2, menu, 1, 0) &&
            !input.ProcessButton(key + 2, menu, 1, 1) &&
            input.ProcessButton(key + 2, menu, 0, 1), "menu hotkeys still activate on paired release");

        press();
        input.ProcessButton(key + 2, menu, 1, 0);
        const auto block = input.AcquireBlock();
        const auto nested = input.AcquireBlock();
        check(block && nested != block && !press() && HotkeyTasks::pending.size() == 1,
            "blocks reject new presses but leave already submitted tasks intact");
        HotkeyTasks::Run();
        check(one.events.size() == 4 && two.events.size() == 3, "submitted callbacks run even while a block is active");
        input.ReleaseBlock(block);
        check(!press(), "releasing one owner does not lift another owner's block");
        input.ReleaseBlock(nested);
        check(!repeat() && !release() && HotkeyTasks::pending.empty(), "held input during a block is not replayed");
        check(!input.ProcessButton(key + 2, menu, 0, 1), "blocks still cancel held menu presses");
        check(press(), "fresh presses resume after all owners release");
        const auto briefBlock = input.AcquireBlock();
        input.ReleaseBlock(briefBlock);
        HotkeyTasks::Run();
        check(one.events.size() == 5, "an acquire/release before execution does not cancel a submitted task");

        one.onCall = [&] { const auto duringCallback = input.AcquireBlock(); input.ReleaseBlock(duringCallback); };
        press();
        HotkeyTasks::Run();
        check(one.events.size() == 6 && two.events.size() == 5,
            "callbacks run outside the input lock and blocking does not cancel the rest of a task");
        one.onCall = {};

        // Work owns its strings and callback records, with no captured input-state lifetime.
        Receiver detached;
        {
            HotkeyInputState temporary;
            temporary.Initialize({ { "sample", {{ "second", Target::Callback }} } });
            temporary.Register("sample", "second", Receiver::Fired, &detached);
            temporary.ProcessButton(key, second, 1, 0);
        }
        HotkeyTasks::Run();
        check(detached.events == std::vector<std::string>{"sample/second"}, "task captures outlive the input-state test fixture");
        std::cout << checks << '/' << checks << " hotkey callback checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
