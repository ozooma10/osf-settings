#include "Diagnostics/IssueRegistry.h"
#include "Settings/Localization.h"
#include "Settings/SettingsJson.h"
#include "Settings/SettingsStore.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

int TestSettingsVersions()
{
    using namespace OSFSettings;
    using Json = nlohmann::ordered_json;
    namespace fs = std::filesystem;
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    const auto installed = *SettingsVersion::Parse("1.9.0");
    check(*SettingsVersion::Parse("1.10.0") > installed, "minor versions compare numerically");
    check(*SettingsVersion::Parse("1.9.10") > *SettingsVersion::Parse("1.9.2"), "patch versions compare numerically");
    check(*SettingsVersion::Parse("2.0.0") > *SettingsVersion::Parse("1.65535.65535"), "major versions take precedence");
    for (const auto* text : { "0.0.0", "1.0.0", "65535.65535.65535" }) {
        const auto version = SettingsVersion::Parse(text);
        check(version && version->String() == text, "valid release versions round-trip");
    }
    const Json minimal = {{ "groups", Json::object() }};
    std::string error;
    std::optional<SettingsVersion> hint = installed;
    const auto absent = SettingsJson::ParseSchema(minimal, "sample", error, &hint);
    check(absent && error.empty() && !hint && !absent->expectedSettingsVersion, "omitted hints retain existing behavior and reset output");
    for (const auto& invalid : { Json(nullptr), Json(true), Json(1.2), Json(1), Json::array(), Json::object(),
        Json(""), Json("1"), Json("1.2"), Json("1.2.3.4"), Json("1..3"), Json("1.2."), Json(".2.3"),
        Json("01.2.3"), Json("1.02.3"), Json("1.2.03"), Json("-1.2.3"), Json("+1.2.3"),
        Json("1.2.3-beta"), Json("1.2.3+build"), Json("v1.2.3"), Json(" 1.2.3"), Json("1.2.3 "),
        Json("65536.0.0"), Json("1.99999999999999999999.0"), Json(std::string("1.2.3\0", 6)) }) {
        auto document = minimal;
        document["expectedSettingsVersion"] = invalid;
        hint = installed;
        check(!SettingsJson::ParseSchema(document, "sample", error, &hint) && !hint &&
            error.find("expectedSettingsVersion") != std::string::npos, "malformed version hints report a schema error without retaining stale output");
    }
    auto document = minimal;
    document["expectedSettingsVersion"] = "1.10.0";
    auto parsed = SettingsJson::ParseSchema(document, "sample", error, &hint);
    check(parsed && error.empty() && parsed->expectedSettingsVersion == hint && hint->String() == "1.10.0",
        "a newer advisory version does not prevent schema loading");
    auto future = document;
    future["groups"]["General"] = Json::array({{{"key", "color"}, {"type", "future-control"}, {"default", "blue"}}});
    std::istringstream input(future.dump());
    check(!SettingsJson::ParseSchema(input, "future", error, &hint) && hint == parsed->expectedSettingsVersion &&
        error.find("only types") != std::string::npos, "source parser retains the hint when newer controls fail validation");
    future = document;
    future["schemaVersion"] = 2;
    check(!SettingsJson::ParseSchema(future, "future", error, &hint) && hint == parsed->expectedSettingsVersion &&
        error.find("schemaVersion") != std::string::npos, "release hint survives an unsupported schema format");
    std::istringstream malformed("{");
    check(!SettingsJson::ParseSchema(malformed, "bad", error, &hint) && !hint, "invalid JSON clears the prior hint");

    const auto root = fs::temp_directory_path() /
        ("osf-settings-version-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup
    {
        fs::path root;
        ~Cleanup()
        {
            std::error_code ignored;
            for (const auto* path : { "schemas/sample.json", "schemas/future.json", "schemas", "" })
                fs::remove(root / path, ignored);
        }
    } cleanup{root};
    fs::create_directories(root / "schemas");
    document["title"] = "Sample Mod";
    document["groups"]["General"] = Json::array({{{"key", "enabled"}, {"type", "bool"}, {"default", true}}});
    future["expectedSettingsVersion"] = "2.0.0";
    { std::ofstream file(root / "schemas/sample.json"); file << document; }
    { std::ofstream file(root / "schemas/future.json"); file << future; }
    SettingsStore store;
    store.LoadAll(root / "schemas", root / "values");
    check(store.Mods().size() == 1 && store.GetValue("sample", "enabled") == SettingValue{true},
        "newer expectations leave supported settings available");
    check(store.LoadErrors().size() == 1 && store.LoadErrors()[0].expectedSettingsVersion->String() == "2.0.0",
        "failed schema version hints survive file discovery");
    const auto warning = SettingsUpdateIssue(store.Mods(), store.LoadErrors(), installed);
    check(warning && warning->modId == "osfsettings" && warning->severity == IssueSeverity::Warning &&
        warning->title == "OSF Settings update recommended", "the combined update notice belongs to OSF Settings");
    check(warning->impact.find("1.9.0") != std::string::npos && warning->impact.find("Sample Mod") != std::string::npos &&
        warning->impact.find("future.json") != std::string::npos && warning->nextSteps.find("2.0.0") != std::string::npos,
        "warning shows the installed version, affected mods including failed schemas, and highest requested version");
    check(SchemaLoadIssues(store.LoadErrors()).size() == 1, "update guidance does not replace the original schema failure");
    IssueRegistry registry;
    check(registry.Report(*warning) && registry.Report(*warning) && registry.Snapshot().size() == 1 &&
        registry.ClearMod("sample") == 0, "repeated update reports replace one OSF-owned issue");
    check(!SettingsUpdateIssue(store.Mods(), store.LoadErrors(), *SettingsVersion::Parse("2.0.0")) &&
        !SettingsUpdateIssue(store.Mods(), store.LoadErrors(), *SettingsVersion::Parse("3.0.0")),
        "equal and newer installations have no update warning");
    check(!SettingsUpdateIssue({}, {}, installed), "no declarations means no update warning");
    const auto partial = SettingsUpdateIssue(store.Mods(), store.LoadErrors(), *SettingsVersion::Parse("1.10.0"));
    check(partial && partial->impact.find("Sample Mod") == std::string::npos && partial->impact.find("future.json") != std::string::npos,
        "only unmet expectations appear in the affected list");
    const std::vector<ModSettings> legacyMods{{ .schema = *absent }};
    check(!SettingsUpdateIssue(legacyMods, {}, installed), "legacy schemas add no expectation");

    const auto originalCatalog = Localization::Get();
    Localization::Publish(std::make_shared<Localization::Catalog>("data/SFSE/Plugins/OSF/Settings/translations", "ja", store.Mods()));
    const auto translated = SettingsUpdateIssue(store.Mods(), store.LoadErrors(), installed);
    Localization::Publish(originalCatalog);
    check(translated && translated->title != warning->title && translated->impact.find("2.0.0") != std::string::npos &&
        translated->impact.find("{mods}") == std::string::npos, "update messages regenerate through the active localization catalog");

    fs::remove(root / "schemas/future.json");
    document.erase("expectedSettingsVersion");
    { std::ofstream file(root / "schemas/sample.json"); file << document; }
    store.LoadAll(root / "schemas", root / "values");
    check(!SettingsUpdateIssue(store.Mods(), store.LoadErrors(), installed), "reloading schemas does not retain removed expectations");
    return checks;
}
