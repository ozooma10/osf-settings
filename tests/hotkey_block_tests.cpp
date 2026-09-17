#include "API/SettingsApi.h"
#include "Input/HotkeyInputState.h"
#include "Settings/SettingsService.h"

#include <array>
#include <barrier>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>

extern "C" void* OSFSettings_RequestAPI(std::uint32_t, std::uint32_t*) noexcept;

int main()
{
    using namespace OSFSettings;
    using API::Status;
    unsigned checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    try {
        HotkeyInputState input;
        SettingsService settings;
        API::SettingsApi adapter(settings, input);
        API::Client client;
        API::HotkeyBlock first = 99, second{};
        check(client.AcquireHotkeyBlock(&first) == Status::NotReady && first == 99 &&
            client.ReleaseHotkeyBlock(first) == Status::NotReady, "disconnected client preserves output");
        check(client.Attach(&adapter) && !client.IsReady(), "attach without loading settings or engine state");
        check(client.AcquireHotkeyBlock(nullptr) == Status::InvalidArgument, "null output is rejected");
        check(client.ReleaseHotkeyBlock(0) == Status::UnknownHotkeyBlock &&
            client.ReleaseHotkeyBlock(99) == Status::UnknownHotkeyBlock, "unknown tokens cannot unblock input");

        constexpr std::uint32_t key = 0x79;
        constexpr auto action = "osfsettings/openMenu";
        const auto press = [&] { return input.ProcessButton(key, action, 1, 0); };
        const auto repeat = [&] { return input.ProcessButton(key, action, 1, 1); };
        const auto release = [&] { return input.ProcessButton(key, action, 0, 1); };
        check(!release() && !repeat() && !release(), "unpaired release and held repeat cannot activate");
        check(!press() && !repeat() && release() && !release(), "one normal press fires once on release");

        press();
        check(client.AcquireHotkeyBlock(&first) == Status::Ok && first != 0,
            "acquire works before settings readiness");
        check(!release() && !press() && !repeat() && !release(), "blocked releases and typing never activate");
        check(client.AcquireHotkeyBlock(&second) == Status::Ok && second && second != first,
            "each owner receives a distinct token");
        check(client.ReleaseHotkeyBlock(first) == Status::Ok &&
            client.ReleaseHotkeyBlock(first) == Status::UnknownHotkeyBlock, "release succeeds exactly once");
        check(!press() && !release(), "one owner cannot release another owner's block");
        check(client.ReleaseHotkeyBlock(second) == Status::Ok && !repeat() && !release(),
            "release after the last block cannot replay typing during capture");
        check(!press() && release(), "new press works after focus closes");

        press();
        client.AcquireHotkeyBlock(&first);
        client.ReleaseHotkeyBlock(first);
        check(!repeat() && !release(), "even a complete block between input events cancels the held opener");
        press();
        client.AcquireHotkeyBlock(&first);
        client.AcquireHotkeyBlock(&second);
        client.ReleaseHotkeyBlock(second);
        check(!press() && !release(), "owners may release in reverse order without lifting the remaining block");
        client.ReleaseHotkeyBlock(first);
        check(!release() && !press() && release(), "nested focus restoration requires a fresh press");

        press();
        check(!input.ProcessButton(key, "anothermod/openMenu", 0, 1) && !release(),
            "a changed action on the same key cannot inherit the previous press");
        press();
        check(!input.ProcessButton(key + 1, action, 0, 1) && release(),
            "a different physical key cannot borrow the held action");
        press();
        check(!input.ProcessButton(key, action, 0, -1) && !release(),
            "cancelled release discards the pending press");
        check(!input.ProcessButton(0, action, 1, 0) && !input.ProcessButton(255, action, 1, 0) &&
            !input.ProcessButton(0xFFFFFFFFu, action, 1, 0) && !input.ProcessButton(key, "", 1, 0),
            "invalid keys and empty actions do not arm input");

        // Keep every token held until all threads acquired one, then release together.
        std::array<API::HotkeyBlock, 8> tokens{};
        std::array<Status, 8> acquired{}, released{};
        std::barrier rendezvous(static_cast<std::ptrdiff_t>(tokens.size() + 1));
        std::array<std::jthread, 8> owners;
        for (std::size_t i = 0; i < owners.size(); ++i) {
            owners[i] = std::jthread([&, i] {
                acquired[i] = client.AcquireHotkeyBlock(&tokens[i]);
                rendezvous.arrive_and_wait();
                rendezvous.arrive_and_wait();
                released[i] = client.ReleaseHotkeyBlock(tokens[i]);
            });
        }
        rendezvous.arrive_and_wait();
        const bool blocked = !press() && !release();
        rendezvous.arrive_and_wait();
        for (auto& owner : owners) owner.join();
        check(blocked && std::set<API::HotkeyBlock>(tokens.begin(), tokens.end()).size() == tokens.size(),
            "concurrent consumers receive unique tokens and keep input blocked");
        for (std::size_t i = 0; i < owners.size(); ++i) {
            check(tokens[i] && acquired[i] == Status::Ok && released[i] == Status::Ok,
                "concurrent acquire/release calls succeed independently");
        }
        check(!press() && release(), "input resumes after all concurrent consumers release");

        std::uint32_t version{};
        auto* exported = static_cast<API::ISettings*>(OSFSettings_RequestAPI(API::kVersion, &version));
        check(exported && version == API::kVersion && client.Attach(exported, version),
            "the production export exposes the extended settings interface");
        auto& productionInput = HotkeyInputState::Get();
        productionInput.ProcessButton(key, action, 1, 0);
        check(client.AcquireHotkeyBlock(&first) == Status::Ok &&
            !productionInput.ProcessButton(key, action, 0, 1), "exported API and native handler share block state");
        check(client.ReleaseHotkeyBlock(first) == Status::Ok &&
            !productionInput.ProcessButton(key, action, 0, 1), "exported release does not revive cancelled input");
        std::cout << checks << '/' << checks << " hotkey block/API checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
