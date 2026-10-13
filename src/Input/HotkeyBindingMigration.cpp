#include "HotkeyBindingMigration.h"

#include <algorithm>
#include <ranges>
#include <vector>

namespace OSFSettings
{
    namespace
    {
        constexpr unsigned char kPrimarySlot = 0;
        constexpr unsigned char kPlaceholderSlot = 2;

        enum class Repair { None, UsePrimaryIdentity, RemoveDuplicate };

        struct SavedBinding
        {
            std::string_view action;
            std::size_t begin;
            std::size_t end;
            std::size_t originalSlotOffset;
            unsigned char currentSlot;
            unsigned char originalSlot; // The saved identity; it does not change when the player rebinds.
            Repair repair = Repair::None;
        };

        std::optional<std::vector<SavedBinding>> ParseBindings(
            std::string_view block, std::span<const std::string> registeredActions)
        {
            const auto byte = [&](std::size_t offset) { return static_cast<unsigned char>(block[offset]); };
            // Version 3 starts with a version byte and a two-byte big-endian block length.
            if (block.size() < 3 || block.size() > 0x4000 || byte(0) != 3 ||
                ((byte(1) << 8) | byte(2)) != block.size()) {
                return std::nullopt;
            }

            std::vector<SavedBinding> bindings;
            for (std::size_t offset = 3; offset < block.size();) {
                const auto begin = offset;
                const auto contextEnd = block.find('\0', offset);
                if (contextEnd == block.npos) return std::nullopt;
                const auto context = block.substr(offset, contextEnd - offset);

                offset = contextEnd + 1;
                const auto actionEnd = block.find('\0', offset);
                if (actionEnd == block.npos) return std::nullopt;
                const auto action = block.substr(offset, actionEnd - offset);

                offset = actionEnd + 1;
                if (offset >= block.size()) return std::nullopt;
                const auto keyCount = byte(offset);
                if (keyCount < 1 || keyCount > 2) return std::nullopt;

                // Skip the 32-bit key codes. Each record ends with current slot, original slot and flags.
                const auto currentSlotOffset = offset + 1 + keyCount * 4;
                const auto end = currentSlotOffset + 3;
                if (end > block.size() || byte(currentSlotOffset) > kPlaceholderSlot) return std::nullopt;
                offset = end;

                if (context == "MainGameplay" && std::ranges::find(registeredActions, action) != registeredActions.end()) {
                    bindings.push_back({action, begin, end, currentSlotOffset + 1,
                        byte(currentSlotOffset), byte(currentSlotOffset + 1)});
                }
            }
            return bindings;
        }

        void ChooseRepair(std::span<SavedBinding> bindings, std::string_view action)
        {
            const SavedBinding* primaryRecord = nullptr;
            SavedBinding* legacyPrimary = nullptr;
            for (auto& binding : bindings) {
                if (binding.action != action) continue;
                // Multiple candidates are ambiguous: leave this action alone.
                if (binding.originalSlot == kPrimarySlot) {
                    if (primaryRecord) return;
                    primaryRecord = &binding;
                } else if (binding.originalSlot == kPlaceholderSlot && binding.currentSlot == kPrimarySlot) {
                    if (legacyPrimary) return;
                    legacyPrimary = &binding;
                }
            }
            // A cleared legacy placeholder has lost its former slot; do not guess.
            if (!legacyPrimary) return;

            if (!primaryRecord) {
                legacyPrimary->repair = Repair::UsePrimaryIdentity;
            } else if (primaryRecord->currentSlot == kPrimarySlot) {
                // The player already saved a current-format primary. It wins over the old one.
                legacyPrimary->repair = Repair::RemoveDuplicate;
            }
            // A current-format clear or alternate stays untouched.
        }
    }

    std::optional<std::string> RepairLegacyHotkeyBindings(
        std::string_view block, std::span<const std::string> registeredActions)
    {
        if (registeredActions.empty()) return std::nullopt;
        auto bindings = ParseBindings(block, registeredActions);
        if (!bindings) return std::nullopt;

        for (const auto& action : registeredActions) {
            ChooseRepair(*bindings, action);
        }

        // Apply edits backwards so removing a duplicate does not move earlier offsets.
        std::optional<std::string> repairedBlock;
        for (const auto& binding : std::views::reverse(*bindings)) {
            if (binding.repair == Repair::None) continue;
            if (!repairedBlock) repairedBlock.emplace(block);
            if (binding.repair == Repair::RemoveDuplicate) {
                repairedBlock->erase(binding.begin, binding.end - binding.begin);
            } else {
                (*repairedBlock)[binding.originalSlotOffset] = kPrimarySlot;
            }
        }
        if (repairedBlock) {
            (*repairedBlock)[1] = static_cast<char>(repairedBlock->size() >> 8);
            (*repairedBlock)[2] = static_cast<char>(repairedBlock->size() & 0xFF);
        }
        return repairedBlock;
    }
}
