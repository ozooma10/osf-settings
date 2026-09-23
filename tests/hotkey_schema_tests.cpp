#include "Input/KeyNames.h"
#include "Settings/SettingsJson.h"
#include "Settings/SettingsStore.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

// Legacy key settings need these symbols. Hotkey declarations must load without
// asking the running game's keyboard to resolve a key name.
namespace OSFSettings
{
    std::optional<std::uint32_t> KeyCodeFromName(std::string_view)
    {
        throw std::runtime_error("unexpected native key lookup during schema parsing");
    }
    bool IsBindableKey(std::uint32_t)
    {
        throw std::runtime_error("unexpected native key validation during schema parsing");
    }
}

int main()
{
    using namespace OSFSettings;
    using Json = nlohmann::ordered_json;
    namespace fs = std::filesystem;
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    try {
        std::ifstream example("data/SFSE/Plugins/OSF/Settings/schemas/osfsettings.json");
        const auto document = Json::parse(example);
        std::string error;
        const auto parsed = SettingsJson::ParseSchema(document, "osfsettings", error);
        check(parsed && error.empty() && parsed->hotkeys.size() == 1, "example declaration loads");
        const auto& action = parsed->hotkeys.front();
        check(action.id == "openMenu" && action.label == "Open mod settings" && action.defaultKey == "F10" && action.menu == "OSFSettingsMenu",
            "identity, label, default name and registered menu target are preserved");
        check(!parsed->FindSetting("openMenu"), "hotkeys are separate from ordinary setting definitions");
        check(parsed->groups.size() == 1 && parsed->groups[0].id == "General" &&
            parsed->groups[0].label == "General" && parsed->groups[0].controls.empty() && action.group == "General",
            "hotkey-only schemas use an implicit General group");

        const Json minimal = {{"groups", Json::object()}};
        const auto identity = SettingsJson::ParseSchema(minimal, "my.mod_2-beta", error);
        check(identity && error.empty() && identity->id == "my.mod_2-beta" && identity->title == identity->id,
            "filename identity needs no root id and supplies the default title");
        for (const auto& id : {std::string{}, std::string("."), std::string(".."), std::string("Uppercase"),
                std::string("bad name"), std::string("../sample"), std::string("日本語"), std::string("mod\0suffix", 10)}) {
            check(!SettingsJson::ParseSchema(minimal, id, error) && error.find("mod id") != std::string::npos,
                "filename identities obey the existing mod ID validation");
        }
        auto legacy = minimal;
        legacy["id"] = "my.mod_2-beta";
        check(SettingsJson::ParseSchema(legacy, "my.mod_2-beta", error).has_value() && error.empty(),
            "matching legacy root id remains accepted");
        for (const auto& id : {Json("different"), Json("MY.MOD_2-BETA"), Json(""), Json(nullptr),
                Json(true), Json(1), Json::array(), Json::object()}) {
            legacy["id"] = id;
            check(!SettingsJson::ParseSchema(legacy, "my.mod_2-beta", error) && !error.empty(),
                "malformed or mismatched legacy root id is rejected");
        }

        auto changed = document;
        changed["hotkeys"]["openMenu"].erase("menu");
        auto result = SettingsJson::ParseSchema(changed, "osfsettings", error);
        check(result && !result->hotkeys[0].menu, "omitting menu declares a callback hotkey without additional schema fields");
        changed = document;
        changed["hotkeys"]["openMenu"].erase("default");
        result = SettingsJson::ParseSchema(changed, "osfsettings", error);
        check(result && !result->hotkeys[0].defaultKey, "omitting the default declares an unbound action");
        changed["hotkeys"] = Json::object();
        result = SettingsJson::ParseSchema(changed, "osfsettings", error);
        check(result && result->hotkeys.empty() && result->groups.empty(), "an empty hotkey object adds no implicit group");
        changed.erase("hotkeys");
        result = SettingsJson::ParseSchema(changed, "osfsettings", error);
        check(result && result->hotkeys.empty(), "existing schemas need no hotkeys field");

        const auto reject = [&](const Json& bad) {
            check(!SettingsJson::ParseSchema(bad, "osfsettings", error) && !error.empty(), "malformed declaration must report a schema error");
        };
        for (const auto& value : {Json(nullptr), Json(true), Json(1), Json("hotkey"), Json::array(),
                Json::array({{{"id", "openMenu"}, {"label", "Open"}}})}) {
            changed = document; changed["hotkeys"] = value; reject(changed);
            check(error == "hotkeys must be an object keyed by id", "hotkeys require an object, including when empty");
        }
        for (const auto& value : {Json(nullptr), Json(true), Json(1), Json("hotkey"), Json::array()}) {
            changed = document; changed["hotkeys"]["openMenu"] = value; reject(changed);
            check(error == "each hotkey must be an object", "hotkey values must be declarations");
        }
        changed = document; changed["hotkeys"]["openMenu"].erase("label"); reject(changed);
        for (const auto& value : {Json(nullptr), Json(true), Json(1), Json(""), Json::array(), Json::object()}) {
            changed = document; changed["hotkeys"]["openMenu"]["label"] = value; reject(changed);
        }
        for (const auto& id : {std::string{}, std::string("open menu"), std::string("bad/id"),
                std::string("日本語"), std::string("open\0Menu", 9)}) {
            changed = document;
            changed["hotkeys"] = Json::object({{id, {{"label", "Invalid ID"}}}});
            reject(changed);
        }
        for (const auto* id : {"openMenu", "OPENMENU"}) {
            changed = document;
            changed["hotkeys"]["openMenu"]["id"] = id;
            reject(changed);
            check(error == "hotkey id must be the object key: openMenu", "nested IDs cannot repeat or override the object key");
        }
        changed = document; changed["hotkeys"]["openMenu"]["default"] = std::string("F10\0hidden", 10); reject(changed);
        changed = document;
        changed["hotkeys"]["OPENMENU"] = {{"label", "Duplicate"}};
        reject(changed);
        check(error == "duplicate hotkey id: OPENMENU", "object keys must remain unique ignoring ASCII case");
        changed = document;
        changed["hotkeys"]["second_action-2"] = {{"label", "Second action"}};
        result = SettingsJson::ParseSchema(changed, "osfsettings", error);
        check(result && result->hotkeys.size() == 2 && result->hotkeys[0].id == "openMenu" &&
            result->hotkeys[1].id == "second_action-2" && !result->hotkeys[1].defaultKey && error.empty(),
            "distinct actions retain order and clear a previous parse error");

        changed["groups"] = Json::object({
            {"Z first page / détails", Json::array()},
            {"A second page", Json::array()}
        });
        changed["hotkeys"]["second_action-2"]["group"] = "A second page";
        result = SettingsJson::ParseSchema(changed, "osfsettings", error);
        check(result && result->groups.size() == 2 && result->groups[0].id == "Z first page / détails" && result->groups[0].label == "Z first page / détails" &&
            result->hotkeys[0].group == "Z first page / détails" && result->hotkeys[1].group == "A second page",
            "omitted groups use the first declared group, while explicit groups retain their target");
        changed["groups"]["Z first page / détails"] = Json::array({{{"key", "enabled"}, {"type", "bool"}, {"default", true}}});
        result = SettingsJson::ParseSchema(changed, "osfsettings", error);
        check(result && result->hotkeys[0].group == "Z first page / détails" && result->groups[0].controls.size() == 1,
            "ordinary settings and hotkeys share a group");
        check(!std::get<SettingDefinition>(result->groups[0].controls[0]).requiresRestart, "settings omit the restart notice by default");
        changed["groups"]["Z first page / détails"][0]["requires"] = "restart";
        result = SettingsJson::ParseSchema(changed, "osfsettings", error);
        check(result && std::get<SettingDefinition>(result->groups[0].controls[0]).requiresRestart, "restart metadata is retained on an ordinary setting");
        for (const auto& value : {Json(nullptr), Json(false), Json(1), Json(""), Json("reload"), Json("Restart"), Json::array()}) {
            auto invalid = changed;
            invalid["groups"]["Z first page / détails"][0]["requires"] = value;
            reject(invalid);
        }
        changed["hotkeys"]["second_action-2"]["group"] = "missing";
        reject(changed);
        check(error == "unknown hotkey group: missing", "unknown group names report the invalid reference");
        changed["hotkeys"]["second_action-2"]["group"] = "a second page";
        reject(changed);
        changed = document;
        changed["hotkeys"]["openMenu"]["group"] = "general";
        reject(changed);
        check(error == "unknown hotkey group: general", "explicit groups must reference a declared group");

        for (const auto& value : {Json(nullptr), Json(true), Json(1), Json("Panel"), Json::array()}) {
            changed = document; changed["groups"] = value; reject(changed);
            check(error == "groups must be an object", "groups require a dictionary, including when empty");
        }
        changed = document; changed.erase("groups"); reject(changed);
        for (const auto& value : {Json(nullptr), Json(true), Json(1), Json("setting"), Json::object()}) {
            changed = document; changed["groups"] = {{"Panel", value}}; reject(changed);
            check(error == "group controls must be an array: Panel", "each group contains a controls array");
        }
        for (const auto& name : {std::string{}, std::string("Panel\0suffix", 12)}) {
            changed = document; changed["groups"] = {{name, Json::array()}}; reject(changed);
        }
        const auto parseText = [&](const char* source) {
            std::istringstream input(source);
            return SettingsJson::ParseSchema(input, "osfsettings", error);
        };
        for (const auto& version : {Json(0), Json(-1), Json(2), Json(1.0), Json("1"), Json(true), Json(nullptr), Json::array(), Json::object()}) {
            changed = document;
            changed["schemaVersion"] = version;
            check(!parseText(changed.dump().c_str()) && error.find("schemaVersion") != std::string::npos,
                "explicit schema versions must be the integer 1");
        }
        changed = document;
        changed.erase("schemaVersion");
        result = parseText(changed.dump().c_str());
        check(result && error.empty() && result->id == parsed->id && result->hotkeys.size() == 1 &&
            result->hotkeys[0].id == action.id && result->hotkeys[0].defaultKey == action.defaultKey &&
            result->hotkeys[0].menu == action.menu,
            "source parsing defaults an omitted schemaVersion to version 1 and clears errors");
        result = parseText(R"({"schemaVersion":1,"groups":{"Z page / 日本語":[],"A page":[]},
            "hotkeys":{"zDefault":{"label":"Default"},"aExplicit":{"label":"Explicit","group":"A page"}}})");
        check(result && error.empty() && result->groups.size() == 2 &&
            result->groups[0].id == "Z page / 日本語" && result->groups[0].label == result->groups[0].id &&
            result->groups[1].id == "A page" && result->hotkeys.size() == 2 &&
            result->hotkeys[0].id == "zDefault" && result->hotkeys[0].group == result->groups[0].id &&
            result->hotkeys[1].id == "aExplicit" && result->hotkeys[1].group == "A page",
            "source parsing preserves authored order and Unicode names");
        for (const auto* source : {
            R"({"groups":{},"hotkeys":{"openMenu":{"label":"First"},"openMenu":{"label":"Second"}}})",
            R"({"groups":{},"hotkeys":{"openMenu":{"label":"First"},"open\u004denu":{"label":"Second"}}})",
            R"({"hotkeys":{"OPENMENU":{"label":"First"},"openMenu":{"label":"Second"}},"groups":{}})"
        }) {
            check(!parseText(source) && error == "duplicate hotkey id: openMenu",
                "duplicate source IDs cannot overwrite hotkeys, including escaped and case-folded keys");
        }
        result = parseText(R"({"hotkeys":{"same":{"label":"same","group":"same"},"other":{"label":"Other","group":"same"}},
            "groups":{"same":[]},"menus":{"same":{"title":"same","menu":"SameMenu"}}})");
        check(result && error.empty() && result->hotkeys.size() == 2 && result->hotkeys[0].id == "same" &&
            result->hotkeys[1].id == "other" && result->groups[0].id == "same" && result->menus[0].id == "same",
            "duplicate detection is scoped to hotkey keys and ignores declaration fields and other sections");
        for (const auto* path : {"examples/hotkeys/osfsettings-hotkeys-example.json", "examples/papyrus/papyrusexample.json",
                "examples/localization/schemas/localization-example.json"}) {
            std::ifstream input(path);
            result = SettingsJson::ParseSchema(input, fs::path(path).stem().string(), error);
            check(result && error.empty() && result->hotkeys.size() == 1,
                "shipped hotkey examples load through the source parser");
        }
        for (const auto* source : {
            R"({"schemaVersion":1,"groups":{"Panel":[],"Panel":[]}})",
            R"({"schemaVersion":1,"groups":{"Panel":[],"\u0050anel":[]}})"
        }) {
            check(!parseText(source) && error == "duplicate group name: Panel", "duplicate source names cannot overwrite a group");
        }
        check(!parseText(R"({"schemaVersion":1,"groups":{"Panel":[]})") && !error.empty(),
            "malformed JSON reports a load error");
        result = parseText(R"({"schemaVersion":1,"groups":{"Panel":[],"panel":[]}})");
        check(result && result->groups.size() == 2 && error.empty(), "group names are case-sensitive and successful parsing clears errors");
        result = parseText(R"({"schemaVersion":1,"groups":{"Panel":[{"key":"enabled","type":"bool","default":true}],
            "Other":[{"key":"enabled","type":"bool","default":false}]}})");
        check(!result && error == "duplicate setting key: enabled", "setting keys remain unique across groups");

        const auto menuDocument = Json::parse(R"({"schemaVersion":1,"title":"Sample Mod","groups":{},
            "menus":{"panel":{"title":"Sample Panel","description":"Open the panel.","menu":"SampleMenu"}}})");
        result = SettingsJson::ParseSchema(menuDocument, "osfsettings", error);
        check(result && result->menus.size() == 1 && result->menus[0].id == "panel" && result->menus[0].menu == "SampleMenu" &&
            result->menus[0].title == "Sample Panel" && result->menus[0].description == "Open the panel.", "menu-only schema retains launcher metadata");
        check(result->groups.empty() && !result->FindSetting("panel"), "menus create neither implicit groups nor setting values");
        changed = menuDocument; changed["menus"]["panel"].erase("description");
        result = SettingsJson::ParseSchema(changed, "osfsettings", error);
        check(result && result->menus[0].description.empty(), "menu description is optional");
        changed["menus"] = Json::object();
        check(SettingsJson::ParseSchema(changed, "osfsettings", error)->menus.empty(), "empty menu object is valid");
        for (const auto& value : {Json(nullptr), Json(true), Json(1), Json("menu"), Json::array(),
                Json::array({{{"id", "panel"}, {"title", "Sample Panel"}, {"menu", "SampleMenu"}}})}) {
            changed = menuDocument; changed["menus"] = value; reject(changed);
            check(error == "menus must be an object keyed by id", "menus require an object, including when empty");
        }
        for (const auto& value : {Json(nullptr), Json(true), Json(1), Json("menu"), Json::array()}) {
            changed = menuDocument; changed["menus"]["panel"] = value; reject(changed);
            check(error == "each menu must be an object", "menu values must be declarations");
        }
        for (const auto* field : {"title", "menu"}) {
            changed = menuDocument; changed["menus"]["panel"].erase(field); reject(changed);
        }
        for (const auto* id : {"panel", "other"}) {
            changed = menuDocument; changed["menus"]["panel"]["id"] = id; reject(changed);
            check(error == "menu id must be the object key: panel", "nested IDs cannot repeat or override the object key");
        }
        for (const auto& id : {std::string{}, std::string("bad\nid"), std::string("hidden\0id", 9), std::string(257, 'x')}) {
            changed = menuDocument;
            changed["menus"] = Json::object({{id, {{"title", "Invalid ID"}, {"menu", "SampleMenu"}}}});
            reject(changed);
        }
        changed = menuDocument; changed["menus"]["panel"]["title"] = std::string("hidden\0text", 11); reject(changed);
        changed = menuDocument; changed["menus"]["panel"]["description"] = "bad\ntext"; reject(changed);
        changed = menuDocument; changed["menus"]["panel"]["menu"] = "OSFSettingsMenu"; reject(changed);
        changed = menuDocument; changed["title"] = std::string(257, 'x'); reject(changed);
        result = parseText(R"({"groups":{},"menus":{"zPanel":{"title":"Z","menu":"ZMenu"},"Panel":{"title":"Upper","menu":"UpperMenu"},
            "panel":{"title":"Lower","menu":"LowerMenu"}}})");
        check(result && error.empty() && result->menus.size() == 3 && result->menus[0].id == "zPanel" &&
            result->menus[1].id == "Panel" && result->menus[2].id == "panel" && result->menus[2].menu == "LowerMenu",
            "menu IDs are case-sensitive and keep authored order");
        for (const auto* source : {
            R"({"groups":{},"menus":{"panel":{"title":"First","menu":"A"},"panel":{"title":"Second","menu":"B"}}})",
            R"({"groups":{},"menus":{"panel":{"title":"First","menu":"A"},"panel":{"title":"Second","menu":"B"}}})",
            R"({"menus":{"panel":{"title":"First","menu":"A"},"panel":{"title":"Second","menu":"B"}},"groups":{}})"
        }) {
            check(!parseText(source) && error == "duplicate menu id: panel",
                "duplicate source IDs cannot overwrite menus, including escaped keys");
        }
        {
            std::ifstream input("examples/launchers/absolute-control.json");
            result = SettingsJson::ParseSchema(input, "absolute-control", error);
            check(result && error.empty() && result->menus.size() == 1 && result->menus[0].id == "panel" &&
                result->menus[0].menu == "AbsoluteControlPanelMenu", "shipped launcher example loads through the source parser");
        }
        check(!SettingsJson::ParseSchema(menuDocument, std::string(129, 'x'), error),
            "launcher mod IDs retain their length limit when supplied by the filename");

        const auto root = fs::temp_directory_path() /
            ("osf-hotkey-schema-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup
        {
            fs::path root;
            ~Cleanup()
            {
                std::error_code ignored;
                for (const auto* path : {"schemas/osfsettings.json", "values/osfsettings.json.tmp",
                        "values/osfsettings.json", "schemas", "values", ""})
                    fs::remove(root / path, ignored);
            }
        } cleanup{root};
        fs::create_directories(root / "schemas");
        changed = document;
        changed["groups"] = Json::object({
            {"Z first page / détails", Json::array({
                {{"key", "enabled"}, {"type", "bool"}, {"default", true}, {"requires", "restart"}}
            })},
            {"A second page", Json::array()}
        });
        changed["menus"] = Json::object({{"enabled", {{"title", "Open panel"}, {"menu", "SampleMenu"}}}});
        { std::ofstream file(root / "schemas/osfsettings.json"); file << changed; }
        SettingsStore store;
        store.LoadAll(root / "schemas", root / "values");
        check(store.LoadErrors().empty() && store.Mods().size() == 1 && store.Mods()[0].schema.hotkeys.size() == 1,
            "normal schema loading retains hotkey declarations");
        check(store.Mods()[0].schema.menus.size() == 1 && store.Mods()[0].schema.menus[0].id == "enabled",
            "normal settings discovery loads menus and allows IDs shared with setting keys");
        check(store.Mods()[0].schema.groups[0].id == "Z first page / détails" &&
            store.Mods()[0].schema.groups[1].id == "A second page" &&
            store.Mods()[0].schema.hotkeys[0].group == "Z first page / détails",
            "file loading preserves declaration order and the default hotkey group");
        check(store.Mods()[0].values.size() == 1 && !store.GetValue("osfsettings", "openMenu"),
            "hotkeys do not create persisted setting values");
        check(store.Mods()[0].schema.FindSetting("enabled")->requiresRestart &&
            store.Set("osfsettings", "enabled", false).ok && store.GetValue("osfsettings", "enabled") == SettingValue{false},
            "restart-required settings still save and publish the new value immediately");
        std::ifstream saved(root / "values/osfsettings.json");
        check(Json::parse(saved)["values"] == Json({{"enabled", false}}), "saving settings excludes hotkey and menu declarations");
        std::cout << checks << '/' << checks << " schema checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
