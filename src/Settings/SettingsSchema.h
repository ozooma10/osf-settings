#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <filesystem>
#include <map>
#include <span>
#include <vector>

#include "SettingValue.h"
#include "SettingsVersion.h"

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
        bool allowMouse{};
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
    // Non-empty ASCII letters, digits, underscores, or hyphens (action, hotkey, and language ids).
    bool IsValidIdentifier(std::string_view id);
    // Lowercases ASCII letters only, for case-insensitive identifier comparison.
    std::string FoldAscii(std::string_view text);
    bool IsValidString(std::string_view text, std::uint32_t maxLength);
    bool IsValidValue(const SettingDefinition& setting, const SettingValue& value);

    struct SettingsGroup
    {
        std::string id;
        std::string label;
        std::vector<std::variant<SettingDefinition, ActionDefinition>> controls;
    };

    using DependencyVersions = std::map<std::string, SettingsVersion>;

    struct ModSchema
    {
        std::string id;
        std::string title;
        std::string description;
        std::vector<SettingsGroup> groups;
        std::vector<HotkeyDefinition> hotkeys;
        std::vector<MenuDefinition> menus;
        std::optional<SettingsVersion> version; // This mod's own release; omitted means unknown.
        std::uint32_t nexusModId{};
        DependencyVersions dependencies; // Minimum versions keyed by settings mod id.
        std::string extends; // Settings mod id whose page shows this mod's controls; empty means its own.
        std::string page; // Resolved page owner; equals id unless extends resolved to another mod.
        SettingDefinition* FindSetting(std::string_view key);
        const SettingDefinition* FindSetting(std::string_view key) const;
        ActionDefinition* FindAction(std::string_view actionId);
        const ActionDefinition* FindAction(std::string_view actionId) const;
        SettingValues DefaultValues() const;
    };

    // How one group's rows are framed on the page they appear on.
    struct GroupFraming
    {
        const ModSchema* page{}; // Page owner; the mod itself unless its controls were contributed.
        std::string label; // Heading label, taken from the host when the group merges into one.
        std::string source; // Contributing mod's title when it alone provides the group; otherwise empty.
    };
    // Schemas are the complete set of loaded mods, so the page owner can be looked up by id.
    GroupFraming FrameGroup(std::span<const ModSchema* const> schemas, const ModSchema& mod, const SettingsGroup& group);
    // Longest chain of extends links followed before a page is treated as unresolvable.
    inline constexpr int MaxPageDepth = 8;
    // Points each schema's page at the mod whose page shows its controls, following extends to the
    // first schema that does not extend another. A missing host, a cycle or a longer chain keeps
    // the schema on its own page; both cases are reported as issues rather than failing the load.
    void ResolvePages(std::span<ModSchema* const> schemas);
    // Indexes into schemas in load order, with each page owner followed by its contributors, so a
    // page's own groups always come before the ones another mod added to it.
    std::vector<std::size_t> PageOrder(std::span<const ModSchema* const> schemas);

    struct ModSettings
    {
        ModSchema schema;
        // File-backed records contain only saved choices. Service snapshots and runtime providers contain complete effective values.
        SettingValues values;
        std::optional<SettingValue> GetValue(std::string_view key) const;
        SettingValues ResolvedValues() const;
    };

    struct SettingsLoadError
    {
        std::filesystem::path file;
        std::string message;
        bool schema{}; // The schema file or directory failed, so the mod has no page.
        DependencyVersions dependencies; // Valid metadata survives unsupported controls/formats.
    };
}
