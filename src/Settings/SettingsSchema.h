#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>

#include "SettingValue.h"

namespace OSFSettings
{
    struct BoolDefinition
    {
        bool defaultValue{};
    };

    struct IntDefinition
    {
        std::int64_t defaultValue{};
        std::optional<std::int64_t> minimum;
        std::optional<std::int64_t> maximum;
    };

    struct FloatDefinition
    {
        double defaultValue{};
        std::optional<double> minimum;
        std::optional<double> maximum;
        double step{ 0.1 }; // Editor increment; stored values need not lie on this grid.
    };

    struct EnumOption
    {
        std::string value;
        std::string label;
    };

    struct EnumDefinition
    {
        EnumValue defaultValue;
        std::vector<EnumOption> options;
    };

    struct KeyDefinition
    {
        KeyBinding defaultValue;
        bool allowUnbound{ true };
    };

    struct StringDefinition
    {
        static constexpr std::uint32_t DefaultMaxLength = 256;
        static constexpr std::uint32_t MaxLength = 4096;
        std::string defaultValue;
        std::uint32_t maxLength{ DefaultMaxLength }; // UTF-8 bytes, excluding NUL.
    };

    struct HotkeyDefinition
    {
        std::string id;
        std::string label;
        std::optional<std::string> defaultKey; // Native key name; omitted means unbound.
        std::optional<std::string> menu; // Registered native menu name to show on release.
        std::string group; // Resolved display group; omitted declarations use the first group.
    };

    struct ActionDefinition
    {
        std::string id;
        std::string label;
        std::string hint;
        std::string confirmation; // Empty means invoke without a confirmation dialog.
    };

    struct MenuDefinition
    {
        std::string id;
        std::string title;
        std::string description;
        std::string menu; // Registered native menu name; shown in the Launcher tab.
    };

    struct SettingDefinition
    {
        std::string key;
        std::string label;
        std::string hint;
        bool requiresRestart{};
        std::variant<BoolDefinition, IntDefinition, FloatDefinition, EnumDefinition, KeyDefinition, StringDefinition> definition;

        SettingValue DefaultValue() const;
    };

    bool IsValidModId(std::string_view id);
    bool IsValidString(std::string_view text, std::uint32_t maxLength);
    bool IsValidValue(const SettingDefinition& setting, const SettingValue& value);

    struct SettingsGroup
    {
        std::string id;
        std::string label;
        std::vector<std::variant<SettingDefinition, ActionDefinition>> controls;
    };

    struct ModSchema
    {
        std::string id;
        std::string title;
        std::string description;
        std::vector<SettingsGroup> groups;
        std::vector<HotkeyDefinition> hotkeys;
        std::vector<MenuDefinition> menus;
        SettingDefinition* FindSetting(std::string_view key);
        const SettingDefinition* FindSetting(std::string_view key) const;
        ActionDefinition* FindAction(std::string_view actionId);
        const ActionDefinition* FindAction(std::string_view actionId) const;
    };

    struct ModSettings
    {
        ModSchema schema;
        SettingValues values;
    };

    struct SettingsLoadError
    {
        std::filesystem::path file;
        std::string message;
        bool schema{}; // The schema file or directory failed, so the mod has no page.
    };
}
