#include "SFSE/Impl/PCH.h"
#include "../src/Input/BindingSnapshot.cpp"

#include <iostream>
#include <stdexcept>

namespace
{
    RE::BSService::TaskQueue queue;
    auto* queuePointer = &queue;
    RE::ControlMap* mapPointer{};
    std::vector<RE::BSService::QueuedDelegate*> tasks;
    bool inlineFallback{};
    void GetString(RE::BSStringPool::Entry*& result, const char* text, bool)
    {
        const auto length = std::strlen(text);
        auto* storage = new std::byte[sizeof(RE::BSStringPool::Entry) + length + 1]{};
        result = std::construct_at(reinterpret_cast<RE::BSStringPool::Entry*>(storage));
        result->_length = static_cast<std::uint32_t>(length);
        result->_refCount = 1;
        std::memcpy(result + 1, text, length + 1);
    }
    void ReleaseString(RE::BSStringPool::Entry*& entry)
    {
        if (entry && --entry->_refCount == 0) delete[] reinterpret_cast<std::byte*>(entry);
        entry = nullptr;
    }
    void Submit(RE::BSService::TaskQueue*, RE::BSService::QueuedDelegate** task)
    {
        if (inlineFallback) return;
        tasks.push_back(*task);
        *task = nullptr;
    }
    void Drain()
    {
        for (auto* task : std::exchange(tasks, {})) task->Release();
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
        case 100121: address = reinterpret_cast<std::uintptr_t>(&Submit); break;
        case 938003: address = reinterpret_cast<std::uintptr_t>(&mapPointer); break;
        case 1186742: address = reinterpret_cast<std::uintptr_t>(&GetString); break;
        case 139340: address = reinterpret_cast<std::uintptr_t>(&ReleaseString); break;
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
        // TaskQueue::AddTask explicitly supports inline execution when queueing
        // is disabled or the caller already owns the drain. Mirror that contract.
        inlineFallback = true;
        RequestBindingSnapshot(mailbox);
        check(tasks.empty() && mailbox->Read().status == BindingSnapshot::Status::Unavailable, "inline callback reports missing map");
        inlineFallback = false;
        RequestBindingSnapshot(mailbox);
        check(mailbox->Read().status == BindingSnapshot::Status::Loading, "queued request stays loading until the drain");
        Drain();
        check(mailbox->Read().status == BindingSnapshot::Status::Unavailable, "missing map is unavailable, not unbound");
        RE::ControlMap map{};
        mapPointer = &map;
        RequestBindingSnapshot(mailbox); Drain();
        check(mailbox->Read().status == BindingSnapshot::Status::Unavailable, "missing input context is unavailable");
        RE::ControlMap::InputContext context{};
        map.inputContexts[0] = &context;
        RequestBindingSnapshot(mailbox); Drain();
        check(mailbox->Read().status == BindingSnapshot::Status::Ready, "verified drain can publish an owned empty context");
        {
            std::array<RE::ControlMap::UserEventMapping, 3> mappings{};
            for (auto& mapping : mappings) mapping.eventID = "same/action";
            mappings[0].keyCode = 32;
            mappings[1].keyCode = 0;
            mappings[2].keyCode = 0x8000;
            mappings[2].modifierKeyCode = 0x100;
            struct ArrayView { std::uint32_t size, capacity; RE::ControlMap::UserEventMapping* data; };
            std::array<ArrayView, 3> arrays{ ArrayView{1,1,&mappings[0]}, ArrayView{1,1,&mappings[1]}, ArrayView{1,1,&mappings[2]} };
            static_assert(sizeof(arrays) == sizeof(RE::ControlMap::InputContext));
            map.inputContexts[0] = reinterpret_cast<RE::ControlMap::InputContext*>(arrays.data());
            RequestBindingSnapshot(mailbox); Drain();
            const auto records = mailbox->Read().records;
            check(records.size() == 3 && records[0].device == 0 && records[1].device == 1 && records[2].device == 2 &&
                records[2].key == 0x8000 && records[2].modifier == 0x100,
                "snapshot preserves all device namespaces and both controller chord members");
            map.inputContexts[0] = &context;
        }
        inlineFallback = true;
        RequestBindingSnapshot(mailbox);
        check(tasks.empty() && mailbox->Read().status == BindingSnapshot::Status::Ready, "inline callback publishes an owned snapshot");
        inlineFallback = false;
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
