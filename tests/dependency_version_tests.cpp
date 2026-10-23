#include "Diagnostics/ModIssue.h"
#include "Settings/SettingsJson.h"
#include "Settings/SettingsSchema.h"
#include "Settings/SettingsStore.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>

int TestDependencyVersions()
{
    using namespace OSFSettings;
    using Json = nlohmann::ordered_json;
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    const auto version = [](const char* text) { return *SettingsVersion::Parse(text); };
    const auto has = [](const std::string& text, const char* part) { return text.find(part) != std::string::npos; };
    std::string error;
    const auto parse = [&](const Json& source, std::string_view id = "sample") {
        return SettingsJson::ParseSchema(source, id, error);
    };

    const Json document = {{ "title", "First mod" }, { "version", "1.0.0" }, { "nexusModId", 42 },
        { "dependencies", {{ "osfui", "2.1.0" }} }, { "groups", Json::object() }};
    const auto parsed = parse(document);
    check(parsed && parsed->version == version("1.0.0") && parsed->nexusModId == 42 &&
        parsed->dependencies.size() == 1 && parsed->dependencies.at("osfui") == version("2.1.0"),
        "own version, Nexus ID and dependencies load");

    for (const auto* id : { "../outside", "bad/id", "con", "internal" }) {
        auto invalid = document;
        invalid["dependencies"] = {{ id, "1.0.0" }};
        check(!parse(invalid), "invalid dependency mod IDs are rejected");
    }
    for (const auto& bad : { Json("v2.1.0"), Json("2.1.0-beta"), Json("2.01.0"), Json(2), Json::object() }) {
        auto invalid = document;
        invalid["dependencies"]["osfui"] = bad;
        check(!parse(invalid), "invalid dependency versions are rejected");
        invalid = document;
        invalid["version"] = bad;
        check(!parse(invalid), "invalid own versions are rejected");
    }
    for (const auto& bad : { Json(-1), Json(1.5), Json(4294967296ULL) }) {
        auto invalid = document;
        invalid["nexusModId"] = bad;
        check(!parse(invalid), "invalid Nexus IDs are rejected");
    }
    const auto plain = parse(Json{{ "groups", Json::object() }});
    check(plain && !plain->version && plain->dependencies.empty(), "manifests without the new fields still load");

    auto second = document;
    second["title"] = "Second mod";
    second["dependencies"]["osfui"] = "2.10.0";
    const Json provider = {{ "title", "OSF UI" }, { "version", "2.0.0" }, { "nexusModId", 17711 }, { "groups", Json::object() }};
    std::vector<ModSettings> mods{ { .schema = *parsed }, { .schema = *parse(second, "second") }, { .schema = *parse(provider, "osfui") } };
    auto issues = DependencyIssues(mods);
    check(issues.size() == 1 && issues[0].id == "dependency:osfui" && issues[0].nexusModId == 17711 &&
        issues[0].title == "OSF UI update recommended", "an old provider is reported with its own mod page");
    check(issues[0].affectedMods == std::vector<std::string>{ "First mod", "Second mod" } && has(issues[0].nextSteps, "2.10.0") &&
        issues[0].installedVersion == "2.0.0" && issues[0].reason.empty() && !issues[0].note.empty(),
        "all unmet consumers are attributed and the highest version is requested");
    mods.back().schema.version = version("2.1.0");
    issues = DependencyIssues(mods);
    check(issues.size() == 1 && issues[0].affectedMods == std::vector<std::string>{ "Second mod" }, "satisfied consumers are omitted");
    for (const auto* current : { "2.10.0", "3.0.0" }) {
        mods.back().schema.version = version(current);
        check(DependencyIssues(mods).empty(), "equal and newer providers have no warning");
    }
    mods.back().schema.version.reset();
    check(DependencyIssues(mods).empty(), "an unversioned provider cannot be compared and is not reported");
    mods.pop_back();
    issues = DependencyIssues(mods);
    check(issues.size() == 1 && issues[0].title == "osfui is missing" && issues[0].nexusModId == 0 &&
        has(issues[0].nextSteps, "2.10.0") && has(issues[0].reason, "osfui.json"), "a missing provider is reported by id");
    check(DependencyIssues({}).empty(), "no declarations produces no warnings");

    // OSF Settings is an ordinary dependency once the running version is stamped on its manifest.
    std::ifstream builtIn("data/SFSE/Plugins/OSF/Settings/schemas/osfsettings.json");
    std::vector<ModSettings> stack{ { .schema = *SettingsJson::ParseSchema(builtIn, "osfsettings", error) } };
    stack.back().schema.version = version("1.1.2");
    auto consumer = document;
    consumer["dependencies"] = {{ "osfsettings", "1.2.0" }};
    stack.push_back({ .schema = *parse(consumer) });
    issues = DependencyIssues(stack);
    check(issues.size() == 1 && issues[0].id == "dependency:osfsettings" && issues[0].title == "OSF Settings update recommended" &&
        issues[0].installedVersion == "1.1.2" && has(issues[0].nextSteps, "1.2.0"), "an old OSF Settings is reported like any provider");
    stack.front().schema.version = version("1.2.0");
    check(DependencyIssues(stack).empty(), "a current OSF Settings satisfies its consumers");

    DependencyVersions retained;
    auto future = consumer;
    future["dependencies"]["osfsettings"] = "2.0.0";
    future["groups"]["General"] = Json::array({{{"key", "new"}, {"type", "future-control"}, {"default", true}}});
    check(!SettingsJson::ParseSchema(future, "future", error, &retained) && retained.at("osfsettings") == version("2.0.0"),
        "unsupported controls retain validated dependency metadata");
    auto newerFormat = future;
    newerFormat["schemaVersion"] = 2;
    check(!SettingsJson::ParseSchema(newerFormat, "future", error, &retained) &&
        has(error, "schemaVersion") && retained.at("osfsettings") == version("2.0.0"),
        "unsupported schema formats retain update guidance");
    std::istringstream duplicate(R"({"groups":{"General":[],"General":[]},"dependencies":{"osfsettings":"2.0.0"}})");
    check(!SettingsJson::ParseSchema(duplicate, "future", error, &retained) &&
        has(error, "duplicate group") && retained.at("osfsettings") == version("2.0.0"),
        "duplicate groups still fail without losing later dependency metadata");
    auto malformedDependencies = future;
    malformedDependencies["dependencies"]["osfui"] = "invalid";
    check(!SettingsJson::ParseSchema(malformedDependencies, "future", error, &retained) && retained.empty(),
        "invalid dependency metadata does not leak a partially validated map");
    retained["stale"] = version("1.0.0");
    std::istringstream malformedJson(R"({"dependencies":{"osfsettings":"2.0.0"},)");
    check(!SettingsJson::ParseSchema(malformedJson, "future", error, &retained) && retained.empty(),
        "malformed JSON clears previous metadata without inventing dependencies");
    retained["stale"] = version("1.0.0");
    check(SettingsJson::ParseSchema(Json{{"groups", Json::object()}}, "plain", error, &retained).has_value() && retained.empty(),
        "omitted dependencies clear previous metadata");

    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
        ("osf-dependencies-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path root;
        ~Cleanup() {
            std::error_code ignored;
            for (const auto* path : {"schemas/osfsettings.json", "schemas/future.json", "schemas", ""})
                fs::remove(root / path, ignored);
        }
    } cleanup{root};
    fs::create_directories(root / "schemas");
    { std::ofstream out(root / "schemas/osfsettings.json"); out << R"({"version":"1.2.0","groups":{}})"; }
    { std::ofstream out(root / "schemas/future.json"); out << future; }
    SettingsStore failedStore;
    failedStore.LoadAll(root / "schemas", root / "values");
    check(failedStore.Mods().size() == 1 && failedStore.LoadErrors().size() == 1 &&
        failedStore.LoadErrors()[0].dependencies.at("osfsettings") == version("2.0.0"),
        "file discovery retains a failed consumer's dependencies");
    issues = DependencyIssues(failedStore.Mods(), failedStore.LoadErrors());
    check(issues.size() == 1 && issues[0].id == "dependency:osfsettings" &&
        issues[0].affectedMods == std::vector<std::string>{ "future.json" } && issues[0].requiredVersion == "2.0.0" &&
        SchemaLoadIssues(failedStore.LoadErrors()).size() == 1,
        "a broken consumer reports both its schema error and actionable update guidance");
    auto satisfied = failedStore.Mods();
    satisfied[0].schema.version = version("2.0.0");
    check(DependencyIssues(satisfied, failedStore.LoadErrors()).empty(), "failed consumers do not warn about satisfied dependencies");
    { std::ofstream out(root / "schemas/future.json"); out << R"({"groups":{}})"; }
    failedStore.LoadAll(root / "schemas", root / "values");
    check(failedStore.LoadErrors().empty() && DependencyIssues(failedStore.Mods(), failedStore.LoadErrors()).empty(),
        "repairing and reloading a schema removes its obsolete dependency warnings");

    for (const auto* file : { "examples/dependencies/osf-stack.json", "examples/dependencies/sample-pack.json" }) {
        std::ifstream input(file);
        check(SettingsJson::ParseSchema(input, std::filesystem::path(file).stem().string(), error).has_value(),
            "shipped dependency examples load through the source parser");
    }
    return checks;
}
