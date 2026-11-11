#include "Diagnostics/ModIssue.h"
#include "Settings/SettingsJson.h"
#include "Settings/SettingsSchema.h"
#include "Settings/SettingsStore.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

int TestExtensionPages()
{
    using namespace OSFSettings;
    using Json = nlohmann::ordered_json;
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    std::string error;
    const auto parse = [&](const Json& source, std::string_view id) {
        auto schema = SettingsJson::ParseSchema(source, id, error);
        if (!schema) throw std::runtime_error("schema " + std::string(id) + " failed to parse: " + error);
        return *schema;
    };
    const auto setting = [](const char* key) {
        return Json::array({ Json{{ "key", key }, { "type", "bool" }, { "default", true }} });
    };

    const Json host = {{ "title", "Base Mod" }, { "groups", {{ "Combat", setting("recoil") }, { "Audio", setting("volume") }} }};
    const Json joins = {{ "title", "Combat Extras" }, { "extends", "basemod" }, { "groups", {{ "Combat", setting("spread") }} }};
    const Json adds = {{ "title", "UI Extras" }, { "extends", "basemod" }, { "groups", {{ "Interface", setting("compass") }} }};

    const auto extension = parse(joins, "basemod-combat");
    check(extension.extends == "basemod" && extension.page == "basemod-combat",
        "extends is parsed and the page starts out as the mod's own");
    check(extension.dependencies.size() == 1 && extension.dependencies.at("basemod") == SettingsVersion{},
        "extending a page implies a dependency on the host");
    auto explicitFloor = joins;
    explicitFloor["dependencies"] = {{ "basemod", "2.0.0" }};
    check(parse(explicitFloor, "basemod-combat").dependencies.at("basemod") == *SettingsVersion::Parse("2.0.0"),
        "an explicit minimum version is not replaced by the implied dependency");

    for (const auto& bad : { Json("basemod-combat"), Json("internal"), Json("Uppercase"), Json("bad/id"), Json(2), Json("") }) {
        auto invalid = joins;
        invalid["extends"] = bad;
        check(!SettingsJson::ParseSchema(invalid, "basemod-combat", error), "invalid extends targets are rejected");
    }

    std::vector<ModSchema> loaded{ parse(host, "basemod"), extension, parse(adds, "basemod-ui") };
    std::vector<ModSchema*> mutableSchemas;
    for (auto& schema : loaded) mutableSchemas.push_back(&schema);
    ResolvePages(mutableSchemas);
    check(loaded[0].page == "basemod" && loaded[1].page == "basemod" && loaded[2].page == "basemod",
        "contributors resolve to the host's page and the host keeps its own");

    const std::vector<const ModSchema*> schemas{ &loaded[0], &loaded[1], &loaded[2] };
    const auto merged = FrameGroup(schemas, loaded[1], loaded[1].groups.front());
    check(merged.page == &loaded[0] && merged.label == "Combat" && merged.source.empty(),
        "a group the host also declares keeps the host's heading and names nobody");
    const auto contributed = FrameGroup(schemas, loaded[2], loaded[2].groups.front());
    check(contributed.page == &loaded[0] && contributed.label == "Interface" && contributed.source == "UI Extras",
        "a group only the contributor declares names the contributor");
    const auto own = FrameGroup(schemas, loaded[0], loaded[0].groups.front());
    check(own.page == &loaded[0] && own.source.empty(), "a page owner's own groups are never attributed");

    const auto order = PageOrder(schemas);
    check(order == std::vector<std::size_t>{ 0, 1, 2 }, "the page owner's rows come before the rows added to it");
    std::vector<ModSchema> reversed{ parse(adds, "a-extra"), parse(host, "zbase") };
    reversed[0].extends = "zbase";
    std::vector<ModSchema*> reversedMutable{ &reversed[0], &reversed[1] };
    ResolvePages(reversedMutable);
    const std::vector<const ModSchema*> reversedSchemas{ &reversed[0], &reversed[1] };
    check(PageOrder(reversedSchemas) == std::vector<std::size_t>{ 1, 0 },
        "a contributor that sorts before its host still follows it");

    // Two contributors landing one group id on the same page leaves it without a single owner.
    std::vector<ModSchema> shared{ parse(host, "basemod"), parse(adds, "basemod-ui"), parse(adds, "basemod-hud") };
    std::vector<ModSchema*> sharedMutable{ &shared[0], &shared[1], &shared[2] };
    ResolvePages(sharedMutable);
    const std::vector<const ModSchema*> sharedSchemas{ &shared[0], &shared[1], &shared[2] };
    check(FrameGroup(sharedSchemas, shared[1], shared[1].groups.front()).source.empty() &&
        FrameGroup(sharedSchemas, shared[2], shared[2].groups.front()).source.empty(),
        "a group two contributors share names neither of them");

    // A cycle and a missing host both keep their own page; only the cycle is an extension issue.
    std::vector<ModSchema> cycle{ parse(joins, "basemod-combat"), parse(host, "basemod") };
    cycle[1].extends = "basemod-combat";
    std::vector<ModSchema*> cycleMutable{ &cycle[0], &cycle[1] };
    ResolvePages(cycleMutable);
    check(cycle[0].page == "basemod-combat" && cycle[1].page == "basemod", "a cycle leaves both mods on their own pages");
    const std::vector<ModSettings> cycleMods{ { .schema = cycle[0] }, { .schema = cycle[1] } };
    auto issues = ExtensionIssues(cycleMods);
    check(issues.size() == 2 && issues[0].id == "extension:basemod-combat" && issues[0].severity == IssueSeverity::Warning,
        "each mod in a cycle is reported once");
    const std::vector<ModSettings> alone{ { .schema = extension } };
    check(ExtensionIssues(alone).empty(), "a missing host is left to the dependency report");
    auto dependency = DependencyIssues(alone);
    check(dependency.size() == 1 && dependency[0].id == "dependency:basemod", "a missing host is reported as a dependency");
    const std::vector<ModSettings> resolved{ { .schema = loaded[0] }, { .schema = loaded[1] } };
    check(ExtensionIssues(resolved).empty() && DependencyIssues(resolved).empty(),
        "an installed host produces no reports at all");

    // An optional host is not a dependency; without it the extension is hidden and nothing is reported.
    auto optionalJoins = joins;
    optionalJoins["extends"] = {{ "mod", "basemod" }, { "optional", true }};
    const auto optional = parse(optionalJoins, "basemod-combat");
    check(optional.extends == "basemod" && optional.extendsOptional && optional.dependencies.empty(),
        "the object form parses an optional host without implying a dependency");
    auto requiredObject = joins;
    requiredObject["extends"] = {{ "mod", "basemod" }};
    const auto required = parse(requiredObject, "basemod-combat");
    check(!required.extendsOptional && required.dependencies.size() == 1, "the object form without optional is a required host");
    for (const auto& bad : { Json::object(), Json{{ "mod", "basemod" }, { "optional", "yes" }}, Json{{ "mod", "basemod-combat" }} }) {
        auto invalid = joins;
        invalid["extends"] = bad;
        check(!SettingsJson::ParseSchema(invalid, "basemod-combat", error), "invalid optional extends objects are rejected");
    }
    std::vector<ModSchema> missingHost{ optional, parse(host, "other") };
    std::vector<ModSchema*> missingMutable{ &missingHost[0], &missingHost[1] };
    ResolvePages(missingMutable);
    const std::vector<const ModSchema*> missingSchemas{ &missingHost[0], &missingHost[1] };
    check(missingHost[0].hidden && !missingHost[1].hidden && PageOrder(missingSchemas) == std::vector<std::size_t>{ 1 },
        "an optional extension without its host is hidden from every page");
    const std::vector<ModSettings> optionalAlone{ { .schema = missingHost[0] } };
    check(ExtensionIssues(optionalAlone).empty() && DependencyIssues(optionalAlone).empty(),
        "a missing optional host is not reported");
    std::vector<ModSchema> presentHost{ parse(host, "basemod"), optional };
    std::vector<ModSchema*> presentMutable{ &presentHost[0], &presentHost[1] };
    ResolvePages(presentMutable);
    check(!presentHost[1].hidden && presentHost[1].page == "basemod", "an installed optional host is joined like a required one");
    auto optionalFloor = optionalJoins;
    optionalFloor["dependencies"] = {{ "basemod", "2.0.0" }};
    auto oldHost = parse(host, "basemod");
    oldHost.version = SettingsVersion::Parse("1.0.0");
    const std::vector<ModSettings> floorMissing{ { .schema = parse(optionalFloor, "basemod-combat") } };
    check(DependencyIssues(floorMissing).empty(), "a version floor on a missing optional host is not reported");
    const std::vector<ModSettings> floorOld{ { .schema = oldHost }, { .schema = parse(optionalFloor, "basemod-combat") } };
    auto outdated = DependencyIssues(floorOld);
    check(outdated.size() == 1 && outdated[0].requiredVersion == "2.0.0", "an installed optional host below its floor is reported");
    DependencyVersions failedDependencies;
    auto failing = optionalFloor;
    failing["groups"] = Json::array();
    check(!SettingsJson::ParseSchema(failing, "basemod-combat", error, &failedDependencies) && failedDependencies.empty(),
        "a failed schema does not report its optional host");

    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
        ("osf-extensions-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path root;
        ~Cleanup() {
            std::error_code ignored;
            fs::remove_all(root, ignored);
        }
    } cleanup{root};
    fs::create_directories(root / "schemas");
    { std::ofstream out(root / "schemas/basemod.json"); out << host; }
    { std::ofstream out(root / "schemas/basemod-combat.json"); out << joins; }
    SettingsStore store;
    store.LoadAll(root / "schemas", root / "values");
    const auto* contributor = store.FindMod("basemod-combat");
    check(store.Mods().size() == 2 && contributor && contributor->schema.page == "basemod",
        "the store resolves pages once every schema has loaded");
    check(store.FindMod("basemod")->schema.FindSetting("spread") == nullptr,
        "a contributed key never becomes part of the host's schema");
    check(store.Set("basemod-combat", "spread", SettingValue{ false }).ok() &&
        store.Set("basemod", "spread", SettingValue{ false }).code == SettingsError::UnknownSetting,
        "writes go to the contributor, not the page owner");
    check(store.ResetMod("basemod").ok() && store.GetValue("basemod-combat", "spread") == SettingValue{ false },
        "resetting the page owner leaves contributed values alone");
    check(fs::exists(root / "values/basemod-combat.json") && !fs::exists(root / "values/basemod.json"),
        "contributed values are stored under the contributor's own mod id");
    return checks;
}
