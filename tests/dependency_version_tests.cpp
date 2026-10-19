#include "Diagnostics/ModIssue.h"
#include "Settings/SettingsJson.h"
#include "Settings/SettingsSchema.h"

#include <fstream>
#include <stdexcept>
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
    check(has(issues[0].impact, "First mod") && has(issues[0].impact, "Second mod") && has(issues[0].nextSteps, "2.10.0"),
        "all unmet consumers and the highest version are shown");
    mods.back().schema.version = version("2.1.0");
    issues = DependencyIssues(mods);
    check(issues.size() == 1 && !has(issues[0].impact, "First mod"), "satisfied consumers are omitted");
    for (const auto* current : { "2.10.0", "3.0.0" }) {
        mods.back().schema.version = version(current);
        check(DependencyIssues(mods).empty(), "equal and newer providers have no warning");
    }
    mods.back().schema.version.reset();
    check(DependencyIssues(mods).empty(), "an unversioned provider cannot be compared and is not reported");
    mods.pop_back();
    issues = DependencyIssues(mods);
    check(issues.size() == 1 && issues[0].title == "osfui is missing" && issues[0].nexusModId == 0 &&
        has(issues[0].nextSteps, "2.10.0"), "a missing provider is reported by id");
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
        has(issues[0].impact, "1.1.2") && has(issues[0].nextSteps, "1.2.0"), "an old OSF Settings is reported like any provider");
    stack.front().schema.version = version("1.2.0");
    check(DependencyIssues(stack).empty(), "a current OSF Settings satisfies its consumers");

    for (const auto* file : { "examples/dependencies/osf-stack.json", "examples/dependencies/sample-pack.json" }) {
        std::ifstream input(file);
        check(SettingsJson::ParseSchema(input, std::filesystem::path(file).stem().string(), error).has_value(),
            "shipped dependency examples load through the source parser");
    }
    return checks;
}
