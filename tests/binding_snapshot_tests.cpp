#include "SFSE/Impl/PCH.h"
#include "../src/Input/BindingSnapshot.cpp"

#include <iostream>
#include <stdexcept>

namespace
{
    RE::BSService::TaskQueue queue;
    auto* queuePointer = &queue;
    std::uint8_t enabled = 1;
    std::uint32_t owner{};
    RE::ControlMap* mapPointer{};
    std::vector<RE::BSService::QueuedDelegate*> tasks;
    bool inlineFallback{};
    void Submit(RE::BSService::TaskQueue*, RE::BSService::QueuedDelegate** task)
    {
        if (inlineFallback) return;
        tasks.push_back(*task);
        *task = nullptr;
    }
    void Drain()
    {
        owner = REX::W32::GetCurrentThreadId();
        for (auto* task : std::exchange(tasks, {})) task->Release();
        owner = 0;
    }
}

namespace REL
{
    IDDB::IDDB() = default;
    std::uint64_t IDDB::offset(std::uint64_t id) const
    {
        std::uintptr_t address{};
        switch (id) {
        case 883606: address = reinterpret_cast<std::uintptr_t>(&queuePointer); break;
        case 810305: address = reinterpret_cast<std::uintptr_t>(&enabled); break;
        case 923104: address = reinterpret_cast<std::uintptr_t>(&owner); break;
        case 100121: address = reinterpret_cast<std::uintptr_t>(&Submit); break;
        case 938003: address = reinterpret_cast<std::uintptr_t>(&mapPointer); break;
        default: throw std::runtime_error("Unexpected relocation " + std::to_string(id));
        }
        return address - REX::FModule::GetExecutingModule().GetBaseAddress();
    }
}

int main()
{
    using namespace OSFSettings;
    unsigned checks{};
    const auto check = [&](bool result, const char* message) {
        if (!result) throw std::runtime_error(message);
        ++checks;
    };
    try {
        auto mailbox = std::make_shared<BindingSnapshot>();
        const auto old = mailbox->Begin();
        const auto latest = mailbox->Begin();
        mailbox->Publish(old, BindingSnapshot::Status::Ready, {{ "obsolete", 0, 0, 0, 65, 255 }});
        check(mailbox->Read().generation == latest && mailbox->Read().status == BindingSnapshot::Status::Loading,
            "obsolete result cannot replace a new loading request");
        mailbox->Publish(latest, BindingSnapshot::Status::Ready, {{ "mod/action", 0, 1, 1, 0, 162 }});
        auto copy = mailbox->Read();
        check(copy.records[0].device == 1 && copy.records[0].key == 0 && copy.records[0].slot == 1 && copy.records[0].modifier == 162,
            "device, mouse zero, alternate slot and modifier remain numeric");
        copy.records[0].action = "modified copy";
        check(mailbox->Read().records[0].action == "mod/action", "published snapshots are owned copies");
        mailbox->Invalidate();
        mailbox->Publish(latest, BindingSnapshot::Status::Ready);
        check(mailbox->Read().status == BindingSnapshot::Status::Unavailable && mailbox->Read().records.empty(), "menu close rejects pending results");
        enabled = 0;
        RequestBindingSnapshot(mailbox);
        check(tasks.empty() && mailbox->Read().status == BindingSnapshot::Status::Unavailable, "disabled queue is unavailable");
        enabled = 1;
        inlineFallback = true;
        // An invalid map pointer proves an off-drain callback never reads mappings.
        mapPointer = reinterpret_cast<RE::ControlMap*>(1);
        RequestBindingSnapshot(mailbox);
        check(mailbox->Read().status == BindingSnapshot::Status::Unavailable, "inline execution on another thread is refused");
        inlineFallback = false;
        RequestBindingSnapshot(mailbox);
        check(mailbox->Read().status == BindingSnapshot::Status::Loading, "queued request stays loading until the drain");
        enabled = 0;
        Drain();
        check(mailbox->Read().status == BindingSnapshot::Status::Unavailable, "queue disabling after submission is guarded");
        enabled = 1;
        mapPointer = nullptr;
        RequestBindingSnapshot(mailbox); Drain();
        check(mailbox->Read().status == BindingSnapshot::Status::Unavailable, "missing map is unavailable, not unbound");
        RE::ControlMap map{};
        mapPointer = &map;
        RequestBindingSnapshot(mailbox); Drain();
        check(mailbox->Read().status == BindingSnapshot::Status::Unavailable, "missing input context is unavailable");
        RE::ControlMap::InputContext context{};
        map.inputContexts[0] = &context;
        RequestBindingSnapshot(mailbox); Drain();
        check(mailbox->Read().status == BindingSnapshot::Status::Ready, "verified drain can publish an owned empty context");
        RequestBindingSnapshot(mailbox);
        mailbox->Invalidate();
        mapPointer = reinterpret_cast<RE::ControlMap*>(1);
        Drain();
        check(mailbox->Read().status == BindingSnapshot::Status::Unavailable, "invalidated queued work never visits the map");
        RequestBindingSnapshot(mailbox);
        std::weak_ptr weak = mailbox;
        mailbox.reset();
        check(weak.expired(), "queued request does not retain the menu mailbox");
        Drain();
        check(tasks.empty(), "destroyed menu request drains safely without engine access");
        std::cout << checks << " snapshot checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
