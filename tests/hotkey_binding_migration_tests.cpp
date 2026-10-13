#include "Input/HotkeyBindingMigration.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    std::string record(std::string_view action, std::uint32_t key, char slot, char metadata,
        std::string_view context = "MainGameplay", std::uint32_t modifier = 0xFF)
    {
        std::string result(context); result += '\0'; result += action; result += '\0';
        result += modifier == 0xFF ? '\1' : '\2';
        const auto code = [&](std::uint32_t value) {
            for (int shift : {24, 16, 8, 0}) result += static_cast<char>((value >> shift) & 0xFF);
        };
        code(key); if (modifier != 0xFF) code(modifier);
        result += slot; result += metadata; result += '\0';
        return result;
    }
    std::string block(std::string_view records)
    {
        const auto size = records.size() + 3;
        std::string result(1, '\3');
        result += static_cast<char>(size >> 8); result += static_cast<char>(size & 0xFF);
        result += records; return result;
    }
}

int main()
{
    unsigned checks{};
    const auto check = [&](bool ok, const char* message) {
        if (!ok) throw std::runtime_error(message);
        ++checks;
    };
    try {
        using OSFSettings::RepairLegacyHotkeyBindings;
        constexpr auto action = "osfsettings/openMenu";
        const std::array<std::string, 1> registeredActions{action};
        const auto migrate = [&](std::string_view saved) { return RepairLegacyHotkeyBindings(saved, registeredActions); };
        const auto vanilla = record("Forward", 0x57, 0, 0) + record("QuickSkills", 0xA1, 1, 2);
        const auto legacy = record(action, 0x7A, 0, 2);
        const auto current = record(action, 0x79, 0, 0);
        const auto alternate = record(action, 0x76, 1, 2);
        const auto cleared = record(action, 0xFF, 2, 2);
        check(migrate(block(vanilla + legacy)) == block(vanilla + record(action, 0x7A, 0, 0)),
            "a legacy F11 override replaces the new F10 default");
        check(migrate(block(vanilla + current + legacy)) == block(vanilla + current),
            "the reported two-primary F10/F11 state keeps the explicit current-format F10 override");
        check(migrate(block(legacy + vanilla + current)) == block(vanilla + current),
            "duplicate repair is independent of physical key sorting and record order");
        check(migrate(block(legacy + alternate)) == block(record(action, 0x7A, 0, 0) + alternate),
            "alternate override survives legacy-primary migration");
        check(!migrate(block(cleared)),
            "an ambiguous cleared placeholder cannot disable a valid default primary");
        check(!migrate(block(cleared + alternate)),
            "a cleared alternate or unused placeholder is not promoted to the primary record");
        check(!migrate(block(record(action, 0xFF, 2, 0) + alternate)),
            "an explicit current-format primary clear stays unchanged alongside an alternate");
        check(migrate(block(record(action, 0x7A, 0, 2, "MainGameplay", 0xA2))) ==
            block(record(action, 0x7A, 0, 0, "MainGameplay", 0xA2)), "chord codes remain intact");
        for (const auto& unchanged : {block(""), block(vanilla), block(current), block(alternate),
                 block(record(action, 0x7A, 2, 0) + legacy),
                 block(record(action, 0x7A, 0, 2, "ShipHUD")),
                 block(record("othermod/openMenu", 0x7A, 0, 2))}) {
            check(!migrate(unchanged), "valid current or unrelated bindings stay unchanged");
        }
        const auto repaired = *migrate(block(vanilla + current + legacy));
        check(!migrate(repaired), "migration is idempotent");

        const std::array<std::string, 3> mods{action, "othermod/openMenu", "callbackmod/toggle"};
        const auto otherLegacy = record(mods[1], 0x75, 0, 2);
        const auto otherCurrent = record(mods[1], 0x76, 0, 0);
        const auto callbackClear = record(mods[2], 0xFF, 2, 2);
        const auto unrelated = record("unregistered/openMenu", 0x73, 0, 2) +
            record(mods[1], 0x74, 0, 2, "ShipHUD");
        const auto multiple = block(otherLegacy + vanilla + legacy + callbackClear + alternate + unrelated);
        const auto multipleExpected = block(record(mods[1], 0x75, 0, 0) + vanilla +
            record(action, 0x7A, 0, 0) + callbackClear + alternate + unrelated);
        check(RepairLegacyHotkeyBindings(multiple, mods) == multipleExpected,
            "multiple mod actions migrate independently while other actions, contexts and alternate records remain intact");
        check(!RepairLegacyHotkeyBindings(multipleExpected, mods), "a migrated multi-mod block is idempotent");
        check(RepairLegacyHotkeyBindings(block(otherLegacy + legacy + otherCurrent + callbackClear + current + unrelated), mods) ==
            block(otherCurrent + callbackClear + current + unrelated),
            "interleaved duplicate removals preserve ambiguous clears and update the complete block length");
        const auto callbackLegacy = record(mods[2], 0x74, 0, 2, "MainGameplay", 0xA2);
        check(RepairLegacyHotkeyBindings(block(otherLegacy + callbackLegacy + otherCurrent + legacy), mods) ==
            block(record(mods[2], 0x74, 0, 0, "MainGameplay", 0xA2) + otherCurrent + record(action, 0x7A, 0, 0)),
            "interleaved removal and metadata changes preserve callback chords and use original offsets");
        check(!RepairLegacyHotkeyBindings(multiple, {}), "no registered actions means no migration");
        const auto ambiguousPrimaries = legacy + record(action, 0x74, 0, 2);
        const auto ambiguousOriginals = current + record(action, 0x73, 0, 0) + legacy;
        for (const auto& ambiguous : {ambiguousPrimaries, ambiguousOriginals}) {
            check(!migrate(block(ambiguous)), "ambiguous action records are not rewritten");
            check(RepairLegacyHotkeyBindings(block(ambiguous + otherLegacy), mods) ==
                block(ambiguous + record(mods[1], 0x75, 0, 0)),
                "one ambiguous action does not prevent an independent action from migrating");
        }
        check(!migrate(block(record(action, 0x75, 1, 0) + legacy)),
            "a current original record used as an alternate is preserved");
        check(RepairLegacyHotkeyBindings(block(record(mods[1], 0x75, 0, 2, "MainGameplay", 0xA2) + otherCurrent), mods) ==
            block(otherCurrent), "duplicate chord removal repairs another mod without matching a particular default key");
        check(!RepairLegacyHotkeyBindings(block(legacy + otherLegacy + std::string("unterminated")), mods),
            "a malformed later record prevents partial migration of an otherwise valid block");
        const auto valid = block(legacy);
        for (std::size_t length = 0; length < valid.size(); ++length)
            check(!migrate(std::string_view(valid).substr(0, length)), "truncated blocks are not rewritten");
        auto malformed = valid; malformed[0] = 4;
        check(!migrate(malformed), "unknown format version is not rewritten");
        malformed = block(std::string("MainGameplay\0unterminated", 25));
        check(!migrate(malformed), "unterminated names are not rewritten");
        malformed = valid; malformed[malformed.size() - 8] = 3;
        check(!migrate(malformed), "unsupported key counts are not rewritten");
        std::cout << checks << '/' << checks << " binding migration checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n'; return 1;
    }
}
