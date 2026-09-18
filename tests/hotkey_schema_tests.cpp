#include "Input/KeyNames.h"
#include "Settings/SettingsJson.h"
#include "Settings/SettingsStore.h"

#include <chrono>
#include <fstream>
#include <iostream>
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
    using Json = nlohmann::json;
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
        const auto parsed = SettingsJson::ParseSchema(document, error);
        check(parsed && error.empty() && parsed->hotkeys.size() == 1, "example declaration loads");
        const auto& action = parsed->hotkeys.front();
        check(action.id == "openMenu" && action.label == "Open mod settings" && action.defaultKey == "F10" && action.menu == "OSFSettingsMenu",
            "identity, label, default name and registered menu target are preserved");
        check(!parsed->FindSetting("openMenu"), "hotkeys are separate from ordinary setting definitions");
        check(parsed->groups.size() == 1 && parsed->groups[0].id == "general" &&
            parsed->groups[0].label == "General" && parsed->groups[0].settings.empty() && action.group == "general",
            "hotkey-only schemas use an implicit General group");

        auto changed = document;
        changed["hotkeys"][0].erase("menu");
        auto result = SettingsJson::ParseSchema(changed, error);
        check(result && !result->hotkeys[0].menu, "omitting menu declares a callback hotkey without additional schema fields");
        changed = document;
        changed["hotkeys"][0].erase("default");
        result = SettingsJson::ParseSchema(changed, error);
        check(result && !result->hotkeys[0].defaultKey, "omitting the default declares an unbound action");
        changed["hotkeys"] = Json::array();
        result = SettingsJson::ParseSchema(changed, error);
        check(result && result->hotkeys.empty() && result->groups.empty(), "an empty hotkey list adds no implicit group");
        changed.erase("hotkeys");
        result = SettingsJson::ParseSchema(changed, error);
        check(result && result->hotkeys.empty(), "existing schemas need no hotkeys field");

        const auto reject = [&](const Json& bad) {
            check(!SettingsJson::ParseSchema(bad, error) && !error.empty(), "malformed declaration must report a schema error");
        };
        for (const auto& value : {Json(nullptr), Json(true), Json(10), Json("F10"), Json::object()}) {
            changed = document; changed["hotkeys"] = value; reject(changed);
            changed = document; changed["hotkeys"][0] = value;
            if (!value.is_object()) reject(changed);
        }
        for (const auto* field : {"id", "label", "default", "menu", "group"}) {
            for (const auto& value : {Json(nullptr), Json(10), Json(false), Json(""), Json(std::string("a\0b", 3))}) {
                changed = document; changed["hotkeys"][0][field] = value; reject(changed);
            }
        }
        for (const auto* field : {"id", "label"}) {
            changed = document; changed["hotkeys"][0].erase(field); reject(changed);
        }
        for (const auto* id : {"open menu", "open.menu", "open\tmenu", "open/menu"}) {
            changed = document; changed["hotkeys"][0]["id"] = id; reject(changed);
        }
        for (const auto* id : {"openMenu", "OPENMENU"}) {
            changed = document;
            changed["hotkeys"].push_back({{"id", id}, {"label", "Duplicate"}});
            reject(changed);
        }
        changed = document;
        changed["hotkeys"].push_back({{"id", "second_action-2"}, {"label", "Second action"}});
        result = SettingsJson::ParseSchema(changed, error);
        check(result && result->hotkeys.size() == 2 && !result->hotkeys[1].defaultKey && error.empty(),
            "distinct actions retain order and clear a previous parse error");

        changed["groups"] = Json::array({
            {{"id", "first"}, {"label", "First page"}, {"settings", Json::array()}},
            {{"id", "second"}, {"settings", Json::array()}}
        });
        changed["hotkeys"][1]["group"] = "second";
        result = SettingsJson::ParseSchema(changed, error);
        check(result && result->groups.size() == 2 && result->groups[0].id == "first" &&
            result->hotkeys[0].group == "first" && result->hotkeys[1].group == "second",
            "omitted groups use the first declared group, while explicit groups retain their target");
        changed["groups"][0]["settings"] = Json::array({{{"key", "enabled"}, {"type", "bool"}, {"default", true}}});
        result = SettingsJson::ParseSchema(changed, error);
        check(result && result->hotkeys[0].group == "first" && result->groups[0].settings.size() == 1,
            "ordinary settings and hotkeys share a group");
        check(!result->groups[0].settings[0].requiresRestart, "settings omit the restart notice by default");
        changed["groups"][0]["settings"][0]["requires"] = "restart";
        result = SettingsJson::ParseSchema(changed, error);
        check(result && result->groups[0].settings[0].requiresRestart, "restart metadata is retained on an ordinary setting");
        for (const auto& value : {Json(nullptr), Json(false), Json(1), Json(""), Json("reload"), Json("Restart"), Json::array()}) {
            auto invalid = changed;
            invalid["groups"][0]["settings"][0]["requires"] = value;
            reject(invalid);
        }
        changed["hotkeys"][1]["group"] = "missing";
        reject(changed);
        check(error == "unknown hotkey group: missing", "unknown group names report the invalid reference");
        changed["hotkeys"][1]["group"] = "Second";
        reject(changed);
        changed = document;
        changed["hotkeys"][0]["group"] = "general";
        reject(changed);
        check(error == "unknown hotkey group: general", "explicit groups must reference a declared group");

        const auto root = fs::temp_directory_path() /
            ("osf-hotkey-schema-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup
        {
            fs::path root;
            ~Cleanup()
            {
                std::error_code ignored;
                for (const auto* path : {"schemas/osfsettings.json", "values/osfsettings.json.tmp", "values/osfsettings.json", "schemas", "values", ""})
                    fs::remove(root / path, ignored);
            }
        } cleanup{root};
        fs::create_directories(root / "schemas");
        changed = document;
        changed["groups"] = Json::array({{{"id", "general"}, {"settings", Json::array({
            {{"key", "enabled"}, {"type", "bool"}, {"default", true}, {"requires", "restart"}}
        })}}});
        { std::ofstream file(root / "schemas/osfsettings.json"); file << changed; }
        SettingsStore store;
        store.LoadAll(root / "schemas", root / "values");
        check(store.LoadErrors().empty() && store.Mods().size() == 1 && store.Mods()[0].schema.hotkeys.size() == 1,
            "normal schema loading retains hotkey declarations");
        check(store.Mods()[0].values.size() == 1 && !store.GetValue("osfsettings", "openMenu"),
            "hotkeys do not create persisted setting values");
        check(store.Mods()[0].schema.FindSetting("enabled")->requiresRestart &&
            store.Set("osfsettings", "enabled", false).ok && store.GetValue("osfsettings", "enabled") == SettingValue{false},
            "restart-required settings still save and publish the new value immediately");
        std::ifstream saved(root / "values/osfsettings.json");
        check(Json::parse(saved)["values"] == Json({{"enabled", false}}), "saving settings excludes hotkey declarations");
        std::cout << checks << '/' << checks << " schema checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
