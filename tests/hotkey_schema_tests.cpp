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
        check(action.id == "openMenu" && action.label == "Open mod settings" && action.defaultKey == "F10",
            "identity, label and default name are preserved");
        check(!parsed->FindSetting("openMenu"), "hotkeys are separate from ordinary setting definitions");

        auto changed = document;
        changed["hotkeys"][0].erase("default");
        auto result = SettingsJson::ParseSchema(changed, error);
        check(result && !result->hotkeys[0].defaultKey, "omitting the default declares an unbound action");
        changed["hotkeys"] = Json::array();
        result = SettingsJson::ParseSchema(changed, error);
        check(result && result->hotkeys.empty(), "an empty hotkey list is allowed");
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
        for (const auto* field : {"id", "label", "default"}) {
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
            {{"key", "enabled"}, {"type", "bool"}, {"default", true}}
        })}}});
        { std::ofstream file(root / "schemas/osfsettings.json"); file << changed; }
        SettingsStore store;
        store.LoadAll(root / "schemas", root / "values");
        check(store.LoadErrors().empty() && store.Mods().size() == 1 && store.Mods()[0].schema.hotkeys.size() == 1,
            "normal schema loading retains hotkey declarations");
        check(store.Mods()[0].values.size() == 1 && !store.GetValue("osfsettings", "openMenu"),
            "hotkeys do not create persisted setting values");
        check(store.Set("osfsettings", "enabled", false).ok, "ordinary settings still save");
        std::ifstream saved(root / "values/osfsettings.json");
        check(Json::parse(saved)["values"] == Json({{"enabled", false}}), "saving settings excludes hotkey declarations");
        std::cout << checks << '/' << checks << " schema checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
