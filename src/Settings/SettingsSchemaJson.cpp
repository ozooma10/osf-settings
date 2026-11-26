#include "SettingsJson.h"
#include "Input/KeyNames.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <nlohmann/json.hpp>

namespace OSFSettings::SettingsJson
{
    namespace
    {
        void Require(bool condition, const std::string& message)
        {
            if (!condition) throw std::runtime_error(message);
        }

        std::string RequiredText(const nlohmann::ordered_json& object, const char* key)
        {
            const auto field = object.find(key);
            Require(field != object.end() && field->is_string(), std::string(key) + " must be a string");
            auto value = field->get<std::string>();
            Require(!value.empty(), std::string(key) + " must not be empty");
            return value;
        }

        std::optional<std::int64_t> ReadInteger(const nlohmann::ordered_json& object, const char* key)
        {
            const auto field = object.find(key);
            if (field == object.end()) return std::nullopt;
            const auto value = DecodeInteger(*field);
            Require(value.has_value(), std::string(key) + " must be a signed 64-bit integer");
            return value;
        }

        std::optional<double> ReadFloat(const nlohmann::ordered_json& object, const char* key)
        {
            const auto field = object.find(key);
            if (field == object.end()) return std::nullopt;
            const auto value = DecodeFloat(*field);
            Require(value.has_value(), std::string(key) + " must be a finite number");
            return value;
        }

        std::string OptionalText(const nlohmann::ordered_json& object, const char* key, const std::string& fallback = {})
        {
            const auto field = object.find(key);
            if (field == object.end()) return fallback;
            Require(field->is_string(), std::string(key) + " must be a string");
            return field->get<std::string>();
        }

        SettingsVersion ReadVersion(const nlohmann::ordered_json& value, const std::string& name)
        {
            Require(value.is_string(), name + " must be a major.minor.patch string");
            const auto version = SettingsVersion::Parse(value.get_ref<const std::string&>());
            Require(version.has_value(), name + " must be major.minor.patch (each component 0-65535, no leading zeros or suffixes)");
            return *version;
        }

        DependencyVersions ReadDependencies(const nlohmann::ordered_json& document)
        {
            DependencyVersions result;
            if (const auto dependencies = document.find("dependencies"); dependencies != document.end()) {
                Require(dependencies->is_object(), "dependencies must be an object mapping settings mod ids to minimum versions");
                for (const auto& [id, minimum] : dependencies->items()) {
                    Require(IsValidModId(id) && id != "internal", "dependency must name a settings mod id: " + id);
                    result[id] = ReadVersion(minimum, "dependency " + id);
                }
            }
            return result;
        }

        // Reads an optional {name: number, ...} object such as a placement's screen or size.
        std::optional<std::pair<double, double>> ReadPair(const nlohmann::ordered_json& object, const char* key, const char* first, const char* second)
        {
            const auto field = object.find(key);
            if (field == object.end()) return std::nullopt;
            Require(field->is_object(), std::string(key) + " must be an object with " + first + " and " + second);
            const auto a = ReadFloat(*field, first);
            const auto b = ReadFloat(*field, second);
            Require(a && b, std::string(key) + " must have numeric " + first + " and " + second);
            return std::pair{ *a, *b };
        }

        PlacementDefinition ReadPlacement(const nlohmann::ordered_json& source)
        {
            PlacementDefinition placement;
            placement.id = RequiredText(source, "id");
            Require(placement.id.size() <= 128 && IsValidIdentifier(placement.id),
                "placement id must use 1-128 ASCII letters, digits, underscores, or hyphens");
            const auto& id = placement.id;
            placement.label = RequiredText(source, "label");
            placement.hint = OptionalText(source, "hint");
            Require(IsValidString(placement.label, 256) && IsValidString(placement.hint, 4096),
                "placement text must be single-line UTF-8 (label: 256 bytes; hint: 4096 bytes): " + id);
            Require(!source.contains("group"), "placements use their containing group; remove the group field: " + id);
            Require(!source.contains("key"), "placements use id, not a setting key: " + id);
            Require(!source.contains("default") && !source.contains("value"), "placements have no value of their own; their bound settings keep the defaults: " + id);
            Require(!source.contains("requires"), "placements do not take requires; mark the bound settings instead: " + id);
            constexpr double limit = 1e9;
            if (const auto screen = ReadPair(source, "screen", "width", "height")) {
                Require(screen->first > 0.0 && screen->second > 0.0 && screen->first <= limit && screen->second <= limit,
                    "screen width and height must be positive: " + id);
                std::tie(placement.screenWidth, placement.screenHeight) = *screen;
                const auto aspect = placement.screenWidth / placement.screenHeight;
                Require(aspect >= 0.25 && aspect <= 8.0, "screen aspect ratio must be between 1:4 and 8:1: " + id);
            }
            if (const auto origin = ReadPair(source, "origin", "x", "y")) {
                Require(origin->first >= 0.0 && origin->first <= 1.0 && origin->second >= 0.0 && origin->second <= 1.0,
                    "origin x and y must be fractions from 0 to 1: " + id);
                std::tie(placement.originX, placement.originY) = *origin;
            }
            const auto bind = source.find("bind");
            Require(bind != source.end() && bind->is_object() && !bind->empty(), "bind must be a non-empty object mapping roles to setting keys: " + id);
            std::set<std::string> keys;
            for (const auto& [role, key] : bind->items()) {
                const auto found = std::ranges::find(PlacementRoles, role);
                Require(found != PlacementRoles.end(), "unknown placement role '" + role + "' (use x, y, left, top, right, bottom, width, height, or scale): " + id);
                Require(key.is_string() && !key.get_ref<const std::string&>().empty(), "bind." + role + " must name a setting key: " + id);
                const auto& name = key.get_ref<const std::string&>();
                Require(keys.insert(FoldAscii(name)).second, "bind uses setting " + name + " for more than one role: " + id);
                placement.bindings[static_cast<std::size_t>(found - PlacementRoles.begin())] = name;
            }
            const auto bound = [&](PlacementRole role) { return !placement.Binding(role).empty(); };
            // An axis saves its size through its size role, or through both of its margins.
            const bool width = bound(PlacementRole::Width) || (bound(PlacementRole::Left) && bound(PlacementRole::Right));
            const bool height = bound(PlacementRole::Height) || (bound(PlacementRole::Top) && bound(PlacementRole::Bottom));
            const bool scale = bound(PlacementRole::Scale);
            Require(!(scale && (width || height)), "bind either scale or a size (width, height, or both margins of an axis), not both: " + id);
            if (const auto size = ReadPair(source, "size", "width", "height")) {
                Require(size->first > 0.0 && size->second > 0.0 && size->first <= limit && size->second <= limit,
                    "size width and height must be positive: " + id);
                std::tie(placement.width, placement.height) = *size;
            } else {
                Require(width && height, "size is required unless both axes bind their size: " + id);
            }
            placement.resize = width || height ? PlacementResize::Free : scale ? PlacementResize::Scale : PlacementResize::None;
            if (const auto resize = source.find("resize"); resize != source.end()) {
                Require(resize->is_string(), "resize must be \"none\", \"scale\", or \"free\": " + id);
                const auto& mode = resize->get_ref<const std::string&>();
                Require(mode == "none" || mode == "scale" || mode == "free", "resize must be \"none\", \"scale\", or \"free\": " + id);
                placement.resize = mode == "free" ? PlacementResize::Free : mode == "scale" ? PlacementResize::Scale : PlacementResize::None;
            }
            Require(placement.resize != PlacementResize::Free || width || height, "resize free needs a size bound (width, height, or both margins of an axis): " + id);
            Require(placement.resize != PlacementResize::Scale || scale || width || height, "resize scale needs scale or a size bound: " + id);
            return placement;
        }

        // Bindings name settings anywhere in the schema, so they are checked once every group has loaded.
        void CheckPlacementBindings(const ModSchema& mod)
        {
            for (const auto& group : mod.groups) for (const auto& control : group.controls) {
                const auto* placement = std::get_if<PlacementDefinition>(&control);
                if (!placement) continue;
                for (std::size_t role = 0; role < PlacementRoles.size(); ++role) {
                    const auto& key = placement->bindings[role];
                    if (key.empty()) continue;
                    const auto* setting = mod.FindSetting(key);
                    const auto where = "bind." + std::string(PlacementRoles[role]) + " of placement " + placement->id;
                    Require(setting, where + " names no setting in this schema: " + key);
                    Require(std::holds_alternative<IntDefinition>(setting->definition) || std::holds_alternative<FloatDefinition>(setting->definition),
                        where + " must name an int or float setting: " + key);
                    if (role == static_cast<std::size_t>(PlacementRole::Scale)) {
                        const auto* floating = std::get_if<FloatDefinition>(&setting->definition);
                        Require(floating, where + " must name a float setting: " + key);
                        Require(floating->defaultValue > 0.0 && (!floating->maximum || *floating->maximum > 0.0), where + " must allow a positive scale: " + key);
                    }
                }
            }
        }

        std::optional<SettingValue> DecodeDefault(const nlohmann::ordered_json& value, const SettingDefinition& setting)
        {
            if (std::holds_alternative<KeyDefinition>(setting.definition) && value.is_string()) {
                const auto code = KeyCodeFromName(value.get_ref<const std::string&>());
                if (!code) return std::nullopt;
                return KeyBinding{ *code };
            }
            return DecodeValue(value, setting);
        }
    }

    std::optional<ModSchema> ParseSchema(std::istream& input, std::string_view modId, std::string& error, DependencyVersions* dependencies)
    {
        error.clear();
        if (dependencies) dependencies->clear();
        try {
            std::set<std::string> groupNames;
            std::set<std::string> hotkeyIds;
            std::set<std::string> menuIds;
            std::set<std::string> optionValues;
            bool inGroups{};
            bool inHotkeys{};
            bool inMenus{};
            bool inOptions{};
            // Finish parsing valid JSON so dependency metadata survives duplicate control names too.
            std::string duplicateError;
            const auto unique = [&](bool inserted, std::string message) {
                if (!inserted && duplicateError.empty()) duplicateError = std::move(message);
            };
            const auto document = nlohmann::ordered_json::parse(input,
                [&](int depth, nlohmann::ordered_json::parse_event_t event, nlohmann::ordered_json& value) {
                    if (event == nlohmann::ordered_json::parse_event_t::key) {
                        if (depth == 1) {
                            inGroups = value == "groups";
                            inHotkeys = value == "hotkeys";
                            inMenus = value == "menus";
                        } else if (depth == 2 && inGroups) {
                            const auto& name = value.get_ref<const std::string&>();
                            unique(groupNames.insert(name).second, "duplicate group name: " + name);
                        } else if (depth == 2 && inHotkeys) {
                            const auto& id = value.get_ref<const std::string&>();
                            unique(hotkeyIds.insert(id).second, "duplicate hotkey id: " + id);
                        } else if (depth == 2 && inMenus) {
                            const auto& id = value.get_ref<const std::string&>();
                            unique(menuIds.insert(id).second, "duplicate menu id: " + id);
                        } else if (depth == 4 && inGroups) {
                            inOptions = value == "options";
                            if (inOptions) {
                                optionValues.clear();
                            }
                        } else if (depth == 5 && inGroups && inOptions) {
                            const auto& option = value.get_ref<const std::string&>();
                            unique(optionValues.insert(option).second, "duplicate option: " + option);
                        }
                    }
                    return true;
                });
            auto schema = ParseSchema(document, modId, error, dependencies);
            if (!duplicateError.empty()) {
                error = std::move(duplicateError);
                return std::nullopt;
            }
            return schema;
        } catch (const std::exception& exception) {
            error = exception.what();
            return std::nullopt;
        }
    }

    std::optional<ModSchema> ParseSchema(const nlohmann::ordered_json& document, std::string_view modId, std::string& error, DependencyVersions* dependencies)
    {
        error.clear();
        if (dependencies) dependencies->clear();
        try {
            Require(document.is_object(), "schema must be an object");
            ModSchema mod;
            mod.id = modId;
            Require(IsValidModId(mod.id), "mod id must use 1-128 lowercase ASCII letters, digits, dots, underscores, or hyphens and must not be a Windows device name");
            // Values share a folder with OSF Settings' internal.json.
            Require(mod.id != "internal", "mod id 'internal' is reserved");
            if (document.contains("id")) {
                Require(RequiredText(document, "id") == mod.id, "schema id must match the filename stem");
            }
            mod.page = mod.id; // Resolved against the other schemas once they have all loaded.
            if (const auto extends = document.find("extends"); extends != document.end()) {
                // A string names a required host; the object form can make the host optional.
                if (extends->is_object()) {
                    mod.extends = RequiredText(*extends, "mod");
                    if (const auto optional = extends->find("optional"); optional != extends->end()) {
                        Require(optional->is_boolean(), "extends optional must be a boolean");
                        mod.extendsOptional = optional->get<bool>();
                    }
                } else {
                    mod.extends = RequiredText(document, "extends");
                }
                Require(IsValidModId(mod.extends), "extends must name a settings mod id: " + mod.extends);
                Require(mod.extends != "internal", "extends must not name the reserved mod id 'internal'");
                Require(mod.extends != mod.id, "extends must not name the schema's own mod id");
            }
            mod.dependencies = ReadDependencies(document);
            // Extending a page needs the host installed, so report it like any other dependency.
            if (!mod.extends.empty() && !mod.extendsOptional) mod.dependencies.emplace(mod.extends, SettingsVersion{});
            if (dependencies) {
                *dependencies = mod.dependencies;
                // A schema that failed to load joins no page, so an optional host is never missing for it.
                if (mod.extendsOptional) dependencies->erase(mod.extends);
            }
            Require(!document.contains("actions"), "top-level actions are no longer supported; move each action into a group with type: action and remove its group field");
            const auto version = document.find("schemaVersion");
            Require(version == document.end() || (version->is_number_integer() && *version == 1), "schemaVersion must be the integer 1 when present");
            mod.title = OptionalText(document, "title", mod.id);
            mod.description = OptionalText(document, "description");
            if (const auto release = document.find("version"); release != document.end()) {
                mod.version = ReadVersion(*release, "version");
            }
            const auto nexusModId = ReadInteger(document, "nexusModId").value_or(0);
            Require(nexusModId >= 0 && nexusModId <= UINT32_MAX, "nexusModId must be an unsigned 32-bit integer");
            mod.nexusModId = static_cast<std::uint32_t>(nexusModId);
            const auto groups = document.find("groups");
            Require(groups != document.end() && groups->is_object(), "groups must be an object");
            std::set<std::string> settingKeys;
            std::set<std::string> actionIds;
            std::set<std::string> placementIds;
            for (const auto& [name, settings] : groups->items()) {
                Require(!name.empty(), "group name must not be empty");
                Require(name.find('\0') == std::string::npos, "group name must not contain NUL");
                SettingsGroup group;
                group.id = name;
                group.label = name;

                Require(settings.is_array(), "group controls must be an array: " + name);
                for (const auto& sourceSetting : settings) {
                    Require(sourceSetting.is_object(), "each control must be an object");
                    const auto type = RequiredText(sourceSetting, "type");
                    if (type == "placement") {
                        auto placement = ReadPlacement(sourceSetting);
                        Require(placementIds.insert(FoldAscii(placement.id)).second, "duplicate placement id: " + placement.id);
                        group.controls.emplace_back(std::move(placement));
                        continue;
                    }
                    if (type == "action") {
                        ActionDefinition action;
                        action.id = RequiredText(sourceSetting, "id");
                        Require(action.id.size() <= 128 && IsValidIdentifier(action.id),
                            "action id must use 1-128 ASCII letters, digits, underscores, or hyphens");
                        Require(actionIds.insert(FoldAscii(action.id)).second, "duplicate action id: " + action.id);
                        action.label = RequiredText(sourceSetting, "label");
                        action.hint = OptionalText(sourceSetting, "hint");
                        action.confirmation = OptionalText(sourceSetting, "confirmation");
                        Require(IsValidString(action.label, 256) && IsValidString(action.hint, 4096) && IsValidString(action.confirmation, 4096),
                            "action text must be single-line UTF-8 (label: 256 bytes; hint/confirmation: 4096 bytes)");
                        if (sourceSetting.contains("confirmation")) Require(!action.confirmation.empty(), "confirmation must not be empty when present");
                        Require(!sourceSetting.contains("group"), "actions use their containing group; remove the group field");
                        Require(!sourceSetting.contains("key"), "actions use id, not a setting key");
                        Require(!sourceSetting.contains("default") && !sourceSetting.contains("value"), "actions do not have a default or value");
                        Require(!sourceSetting.contains("requires"), "actions do not have a restart requirement");
                        group.controls.emplace_back(std::move(action));
                        continue;
                    }
                    SettingDefinition setting;
                    setting.key = RequiredText(sourceSetting, "key");
                    Require(setting.key.find('\0') == std::string::npos, "setting key must not contain NUL");
                    Require(settingKeys.insert(FoldAscii(setting.key)).second, "duplicate setting key: " + setting.key);
                    Require(type == "bool" || type == "int" || type == "float" || type == "enum" || type == "key" || type == "string", "only types bool, int, float, enum, key, string, action, and placement are supported: " + setting.key);
                    std::string defaultError = "default must be a boolean: ";
                    if (type == "string") {
                        defaultError = "default must be valid single-line UTF-8 within maxLength bytes: ";
                        StringDefinition definition;
                        const auto limit = ReadInteger(sourceSetting, "maxLength").value_or(StringDefinition::DefaultMaxLength);
                        Require(limit >= 1 && limit <= StringDefinition::MaxLength, "maxLength must be an integer from 1 to 4096 UTF-8 bytes: " + setting.key);
                        definition.maxLength = static_cast<std::uint32_t>(limit);
                        setting.definition = definition;
                    } else if (type == "int") {
                        defaultError = "default must be an integer within its bounds: ";
                        IntDefinition definition;
                        definition.minimum = ReadInteger(sourceSetting, "min");
                        definition.maximum = ReadInteger(sourceSetting, "max");
                        Require(!definition.minimum || !definition.maximum || *definition.minimum <= *definition.maximum, "min must not exceed max: " + setting.key);
                        setting.definition = definition;
                    } else if (type == "float") {
                        defaultError = "default must be a finite number within its bounds: ";
                        FloatDefinition definition;
                        definition.minimum = ReadFloat(sourceSetting, "min");
                        definition.maximum = ReadFloat(sourceSetting, "max");
                        definition.step = ReadFloat(sourceSetting, "step").value_or(0.1);
                        Require(definition.step > 0.0, "step must be positive: " + setting.key);
                        Require(!definition.minimum || !definition.maximum || *definition.minimum <= *definition.maximum, "min must not exceed max: " + setting.key);
                        setting.definition = definition;
                    } else if (type == "key") {
                        defaultError = "default must be a keyboard virtual-key integer or recognized key name (255/UNBOUND disallowed when allowUnbound is false): ";
                        KeyDefinition definition;
                        const auto mouse = sourceSetting.find("allowMouse");
                        Require(mouse == sourceSetting.end() || mouse->is_boolean(), "allowMouse must be a boolean: " + setting.key);
                        if (mouse != sourceSetting.end()) definition.allowMouse = mouse->get<bool>();
                        const auto unbound = sourceSetting.find("allowUnbound");
                        Require(unbound == sourceSetting.end() || unbound->is_boolean(), "allowUnbound must be a boolean: " + setting.key);
                        if (unbound != sourceSetting.end()) {
                            definition.allowUnbound = unbound->get<bool>();
                        }
                        setting.definition = definition;
                    } else if (type == "enum") {
                        defaultError = "default must be a string matching an option: ";
                        EnumDefinition definition;
                        const auto options = sourceSetting.find("options");
                        Require(options != sourceSetting.end() && (options->is_array() || options->is_object()) && !options->empty(), "options must be a non-empty array or object: " + setting.key);
                        Require(!sourceSetting.contains("optionLabels"), "optionLabels is no longer supported; use an options object: " + setting.key);
                        std::set<std::string> optionValues;
                        for (const auto& [key, value] : options->items()) {
                            EnumOption option;
                            if (options->is_object()) {
                                option.value = key;
                                Require(value.is_string(), "each option label must be a string: " + setting.key);
                                option.label = value.get<std::string>();
                            } else {
                                Require(value.is_string(), "each option must be a string: " + setting.key);
                                option.value = value.get<std::string>();
                            }
                            Require(option.value.find('\0') == std::string::npos, "option value must not contain NUL: " + setting.key);
                            Require(optionValues.insert(FoldAscii(option.value)).second, "duplicate option: " + setting.key + " / " + option.value);
                            if (option.label.empty()) option.label = option.value;
                            definition.options.push_back(std::move(option));
                        }
                        setting.definition = std::move(definition);
                    }
                    const auto value = sourceSetting.find("default");
                    const auto decoded = value != sourceSetting.end() ? DecodeDefault(*value, setting) : std::nullopt;
                    Require(decoded && IsValidValue(setting, *decoded), defaultError + setting.key);
                    std::visit([&](auto& definition) {
                        definition.defaultValue = std::get<decltype(definition.defaultValue)>(*decoded);
                    }, setting.definition);
                    setting.label = OptionalText(sourceSetting, "label", setting.key);
                    setting.hint = OptionalText(sourceSetting, "hint");
                    if (const auto requirement = sourceSetting.find("requires"); requirement != sourceSetting.end()) {
                        Require(requirement->is_string() && *requirement == "restart", "requires must be \"restart\" when present: " + setting.key);
                        setting.requiresRestart = true;
                    }
                    group.controls.emplace_back(std::move(setting));
                }
                mod.groups.push_back(std::move(group));
            }
            CheckPlacementBindings(mod);
            if (const auto hotkeys = document.find("hotkeys"); hotkeys != document.end()) {
                Require(hotkeys->is_object(), "hotkeys must be an object keyed by id");
                std::set<std::string> ids;
                for (const auto& [id, source] : hotkeys->items()) {
                    Require(source.is_object(), "each hotkey must be an object");
                    Require(!source.contains("id"), "hotkey id must be the object key: " + id);
                    HotkeyDefinition hotkey;
                    hotkey.id = id;
                    Require(!hotkey.id.empty(), "hotkey id must not be empty");
                    Require(IsValidIdentifier(hotkey.id), "hotkey id must use ASCII letters, digits, underscores, or hyphens");
                    Require(ids.insert(FoldAscii(hotkey.id)).second, "duplicate hotkey id: " + hotkey.id);
                    hotkey.label = RequiredText(source, "label");
                    Require(hotkey.label.find('\0') == std::string::npos, "hotkey label must not contain NUL");
                    if (source.contains("default")) {
                        hotkey.defaultKey = RequiredText(source, "default");
                        Require(hotkey.defaultKey->find('\0') == std::string::npos, "hotkey default must not contain NUL");
                    }
                    if (source.contains("menu")) {
                        hotkey.menu = RequiredText(source, "menu");
                        Require(hotkey.menu->find('\0') == std::string::npos, "hotkey menu must not contain NUL");
                    }
                    if (source.contains("group")) {
                        hotkey.group = RequiredText(source, "group");
                        Require(groups->contains(hotkey.group), "unknown hotkey group: " + hotkey.group);
                    } else {
                        hotkey.group = mod.groups.empty() ? "General" : mod.groups.front().id;
                    }
                    mod.hotkeys.push_back(std::move(hotkey));
                }
            }
            if (const auto menus = document.find("menus"); menus != document.end()) {
                Require(menus->is_object(), "menus must be an object keyed by id");
                if (!menus->empty()) Require(mod.id.size() <= 128 && IsValidString(mod.title, 256),
                    "launcher owner id must fit 128 bytes and title must be single-line UTF-8 within 256 bytes");
                for (const auto& [id, source] : menus->items()) {
                    Require(source.is_object(), "each menu must be an object");
                    Require(!source.contains("id"), "menu id must be the object key: " + id);
                    MenuDefinition menu;
                    menu.id = id;
                    Require(!menu.id.empty(), "menu id must not be empty");
                    menu.title = RequiredText(source, "title");
                    menu.description = OptionalText(source, "description");
                    menu.menu = RequiredText(source, "menu");
                    Require(IsValidString(menu.id, 256) && IsValidString(menu.title, 256) &&
                        IsValidString(menu.menu, 256) && IsValidString(menu.description, 4096),
                        "menu text must be single-line UTF-8 (id/title/menu: 256 bytes; description: 4096 bytes)");
                    Require(menu.menu != "OSFSettingsMenu", "a launcher cannot open OSFSettingsMenu itself");
                    mod.menus.push_back(std::move(menu));
                }
            }
            if (mod.groups.empty() && !mod.hotkeys.empty()) {
                mod.groups.push_back({ "General", "General", {} });
            }
            return mod;
        } catch (const std::exception& exception) {
            error = exception.what();
            return std::nullopt;
        }
    }
}
