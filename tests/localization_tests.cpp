#include "Settings/Localization.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>

using namespace OSFSettings;
namespace OSFSettings { bool IsBindableKey(std::uint32_t code) { return code > 0 && code < KeyBinding::Unbound; } }

int main()
{
    unsigned checks{};
    const auto check = [&](bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
        ++checks;
    };
    const auto root = std::filesystem::temp_directory_path() / ("osf-localization-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup
    {
        std::filesystem::path root;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(root, error); }
    } cleanup{ root };
    try {
        const auto write = [&](const char* language, const char* mod, const nlohmann::json& json) {
            std::filesystem::create_directories(root / language);
            std::ofstream(root / language / (std::string(mod) + ".json"), std::ios::binary) << json.dump();
        };
        ModSettings mod;
        mod.schema.id = "example";
        mod.schema.title = "Authored title";
        mod.schema.description = "Authored description";
        SettingDefinition setting{ .key = "mode/with.dots", .label = "Mode", .hint = "Original hint", .requiresRestart = true,
            .definition = EnumDefinition{ { "fast" }, {{ "slow", "Slow" }, { "fast", "Fast" }} } };
        mod.schema.groups = {{ "General / group", "General / group", {setting} }};
        mod.schema.hotkeys = {{ "open", "Open", "F10", "Menu", "General / group" }};
        mod.schema.groups[0].controls.emplace_back(ActionDefinition{ "run", "Run", "Hint", "Really run?" });
        mod.schema.groups[0].controls.emplace_back(ActionDefinition{ "quick", "Quick", "", "" });
        mod.values.emplace(setting.key, EnumValue{ "slow" });
        std::vector mods{mod};
        check(Localization::NormalizeLanguage("PTBR") == "ptbr", "game language codes normalize");
        check(Localization::NormalizeLanguage("zhhans") == "zhhans", "game Chinese code remains intact");
        for (const char* invalid : {"", "../de", "C:\\de", "de/fr", "de\n"}) check(Localization::NormalizeLanguage(invalid) == "en", "unsafe language falls back");
        Localization::Catalog missing(root, "ja", mods);
        auto translated = mod.schema;
        missing.Apply(translated);
        check(missing.Errors().empty() && translated.title == mod.schema.title, "missing catalogs preserve authored fields");
        check(missing.UI().at("menu.title") == "MOD SETTINGS", "embedded English works without files or OSF schema");
        write("en", "example", {{"version",1},{"title","English title"},{"settings",{{setting.key,{{"hint","English hint"}}}}}});
        write("de", "example", {{"version",1},{"title","Deutsch"},
            {"groups",{{"General / group",{{"label","Allgemein"}}}}},
            {"settings",{{setting.key,{{"label","Modus"},{"optionLabels",{{"slow","Langsam"},{"fast","Schnell"}}}}}}},
            {"hotkeys",{{"open",{{"label","Öffnen"}}}}},
            {"actions",{{"run",{{"label","Ausführen"},{"confirmation","Wirklich ausführen?"}}}}}});
        Localization::Catalog german(root, "DE", mods);
        german.Apply(translated);
        check(german.Errors().empty() && german.Language() == "de", "valid catalogs load cleanly");
        check(translated.title == "Deutsch" && translated.description == "Authored description", "selected and authored fallbacks");
        check(translated.groups[0].label == "Allgemein" && translated.groups[0].id == "General / group", "group identities stay authored");
        const auto& current = std::get<SettingDefinition>(translated.groups[0].controls[0]);
        check(current.key == setting.key && current.label == "Modus" && current.hint == "English hint" && current.requiresRestart, "exact setting keys and English field fallback");
        const auto& enumeration = std::get<EnumDefinition>(current.definition);
        check(enumeration.defaultValue.value == "fast" && enumeration.options[0].value == "slow" && enumeration.options[0].label == "Langsam", "enum labels do not replace values or order");
        check(IsValidValue(current, EnumValue{"slow"}) && !IsValidValue(current, EnumValue{"Langsam"}), "setters still validate stored enum values");
        check(translated.hotkeys[0].label == "Öffnen" && translated.hotkeys[0].defaultKey == "F10" && translated.hotkeys[0].menu == "Menu", "hotkey localization does not change registration");
        check(translated.FindAction("run")->confirmation == "Wirklich ausführen?" && translated.FindAction("quick")->confirmation.empty(), "confirmation translation preserves enabled state");
        Localization::Catalog english(root, "fr", mods);
        english.Apply(translated);
        check(translated.title == "English title", "missing selected language uses English catalog");

        write("de", "example", {{"version",1},{"title",23},{"description",""},
            {"settings",{{setting.key,{{"hint","Mehrere\nZeilen"},{"label",std::string("bad\0label",9)},{"default","bad"}}},{"missing",{{"label","No"}}}}},
            {"actions",{{"run",{{"confirmation",""}}},{"quick",{{"confirmation","Must not add a dialog"}}}}}});
        Localization::Catalog invalid(root, "de", mods);
        invalid.Apply(translated);
        check(invalid.Errors().size() == 6, "bad entries and unknown fields report diagnostics");
        check(translated.title == "English title" && translated.description.empty(), "bad title falls back and optional text can be cleared");
        check(std::get<SettingDefinition>(translated.groups[0].controls[0]).label == "Mode" && std::get<SettingDefinition>(translated.groups[0].controls[0]).hint == "Mehrere\nZeilen", "invalid NUL rejected and valid multiline sibling retained");
        check(translated.FindAction("run")->confirmation == "Really run?" && translated.FindAction("quick")->confirmation.empty(), "catalog cannot remove or add confirmation requirement");
        std::ofstream(root / "de/example.json") << R"({"version":1,"title":"first","title":"second"})";
        Localization::Catalog duplicate(root, "de", mods);
        duplicate.Apply(translated);
        check(duplicate.Errors().size() == 1 && translated.title == "English title", "duplicate keys reject the file without partial changes");
        std::ofstream(root / "de/example.json") << "{broken";
        Localization::Catalog broken(root, "de", mods);
        broken.Apply(translated);
        check(broken.Errors().size() == 1 && translated.title == "English title", "malformed JSON falls back");

        write("en", "osfsettings", {{"version",1},{"hotkeys",{{"openMenu",{{"label","Open mod settings"}}}}}});
        Localization::Catalog interfaceOnly(root, "en", mods);
        check(interfaceOnly.Errors().empty(), "OSF schema translations are ignored when only the interface is installed");
        write("ja", "osfsettings", {{"version",1},{"ui",{{"menu.title","設定 日本語 é"},{"keys.mouse","{number} マウス"},
            {"strings.bytes","Bad {unknown}"},{"keys.code","Bad {code"},{"missing","unused"},{"menu.default",false}}}});
        Localization::Catalog japanese(root, "ja", mods);
        check(japanese.UI().at("menu.title") == "設定 日本語 é" && japanese.UI().at("keys.mouse") == "{number} マウス", "UTF-8 and reordered placeholders retained");
        check(japanese.Errors().size() == 4 && japanese.UI().at("strings.bytes") == missing.UI().at("strings.bytes"), "unknown keys and invalid placeholders fall back per entry");
        check(Localization::Format(japanese.UI().at("keys.mouse"), {{"number","2"}}) == "2 マウス", "named formatting");
        check(Localization::Format("{value}/{value}", {{"value","$x{other}\\"}}) == "$x{other}\\/$x{other}\\", "substituted text is literal and repeated placeholders work");
        Localization::Publish(std::make_shared<Localization::Catalog>(japanese));
        check(tr("menu.title") == "設定 日本語 é", "published catalog provides native copy");
        Localization::Publish(std::make_shared<Localization::Catalog>());
        std::cout << checks << " localization checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
