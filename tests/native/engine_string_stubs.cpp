#include "SFSE/Impl/PCH.h"
#include "RE/B/BSStringPool.h"

#include <cstring>
#include <stdexcept>

// Supply the string-pool calls and fallback keyboard table used by the key
// fixture. Unexpected engine calls still fail the test.
namespace
{
    void GetStringEntry(RE::BSStringPool::Entry*& result, const char* text, bool)
    {
        const auto length = std::strlen(text);
        auto* storage = new std::byte[sizeof(RE::BSStringPool::Entry) + length + 1];
        auto* entry = std::construct_at(reinterpret_cast<RE::BSStringPool::Entry*>(storage));
        entry->_length = static_cast<std::uint32_t>(length);
        entry->_refCount = 1;
        std::memcpy(entry + 1, text, length + 1);
        result = entry;
    }

    void ReleaseStringEntry(RE::BSStringPool::Entry*& entry)
    {
        if (!entry) return;
        REX::TAtomicRef references{ entry->_refCount };
        if (--references == 0) delete[] reinterpret_cast<std::byte*>(entry);
        entry = nullptr;
    }
}

namespace REL
{
    IDDB::IDDB() = default;

    std::uint64_t IDDB::offset(std::uint64_t id) const
    {
        std::uintptr_t address{};
        if (id == RE::ID::BSStringPool::GetEntry.id()) {
            address = reinterpret_cast<std::uintptr_t>(&GetStringEntry);
        } else if (id == RE::ID::BSStringPool::Entry::Release.id()) {
            address = reinterpret_cast<std::uintptr_t>(&ReleaseStringEntry);
        } else if (id == RE::ID::BSWin32KeyboardDevice::KeyNameTable.id()) {
            address = reinterpret_cast<std::uintptr_t>(L"F4\t73\nSpace\t20\n");
        } else {
            throw std::runtime_error("Unexpected engine relocation in native tests: " + std::to_string(id));
        }
        return address - REX::FModule::GetExecutingModule().GetBaseAddress();
    }
}
