#include "Diagnostics/ModIssue.h"
#include "Diagnostics/IssueRegistry.h"
#include "Input/KeyNames.h"
#include "Settings/SettingsSchema.h"

#include <iostream>
#include <stdexcept>

int TestDiagnosticsService();

// Linking the existing mod-ID validator also brings in key-value validation.
// Issue reporting must not need the game's keyboard.
namespace OSFSettings
{
    bool IsBindableKey(std::uint32_t)
    {
        throw std::runtime_error("unexpected native key validation during issue reporting");
    }
}

int main()
{
    using namespace OSFSettings;
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    try {
        ModIssue issue{ .modId = "sample", .id = "missing-pack" };
        check(IssueModName(issue, {}) == "sample", "a mod without settings uses its ID as its display name");

        std::vector<ModSettings> settings{
            { .schema = { .id = "other", .title = "Other Mod" } },
            { .schema = { .id = "sample", .title = "Sample Mod" } }
        };
        check(IssueModName(issue, settings) == "Sample Mod", "a matching schema supplies the display name");
        check(issue.modId == "sample", "resolving a display name preserves the issue's mod ID");

        const auto previousName = IssueModName(issue, settings);
        settings[1].schema.title = "Mód 日本語";
        check(IssueModName(issue, settings) == "Mód 日本語", "name lookup reflects the current schema title");
        check(previousName == "Sample Mod", "the returned name owns its text");

        settings[1].schema.title.clear();
        check(IssueModName(issue, settings) == "sample", "a schema with no title falls back to the mod ID");
        settings.pop_back();
        check(IssueModName(issue, settings) == "sample", "unrelated schemas do not supply a display name");

        const std::vector<SettingsLoadError> loadErrors{
            { "schemas/broken.json", "unexpected end of input", true },
            { "values/sample.json", "saved value does not match the setting's type or validation rules: gain" },
            { "schemas/Not A Mod ID.json", "schemaVersion must be the integer 1 when present", true }
        };
        const auto schemaIssues = SchemaLoadIssues(loadErrors);
        check(schemaIssues.size() == 2, "only schema failures become issues; saved-value problems stay in the log");
        check(schemaIssues[0].modId == "osfsettings" && schemaIssues[0].id == "schema:broken.json" &&
            schemaIssues[0].severity == IssueSeverity::Error, "OSF owns schema failures as errors keyed by filename");
        check(schemaIssues[0].title.find("broken.json") != std::string::npos &&
            schemaIssues[0].reason == loadErrors[0].message && !schemaIssues[0].nextSteps.empty() &&
            schemaIssues[0].impact.find(loadErrors[0].message) == std::string::npos,
            "a schema issue names the file, separates the exact reason from impact, and says what to do");
        check(schemaIssues[1].id == "schema:Not A Mod ID.json", "files whose names are not valid mod IDs are still reported");
        IssueRegistry schemaRegistry;
        check(schemaRegistry.Report(schemaIssues[0]) && schemaRegistry.Report(schemaIssues[1]) && schemaRegistry.Report(schemaIssues[0]) &&
            schemaRegistry.Snapshot().size() == 2, "reporting schema failures again replaces them instead of duplicating");
        check(schemaRegistry.ClearMod("broken") == 0 && schemaRegistry.Snapshot().size() == 2,
            "a mod clearing its own issues cannot hide an OSF schema failure");

        IssueRegistry registry;
        check(registry.Snapshot().empty(), "a new registry has no active issues");
        ModIssue warning{
            .modId = "sample", .id = "missing-pack", .severity = IssueSeverity::Warning,
            .title = "Custom animations are unavailable", .impact = "Scenes use standard animations.",
            .nextSteps = "Install the animation pack and restart."
        };
        auto input = warning;
        check(registry.Report(input), "a mod can report an issue without registration or a schema");
        input.modId = "changed";
        input.title = "Changed";
        check(registry.Snapshot()[0].modId == "sample" && registry.Snapshot()[0].title == warning.title,
            "the registry owns the reported identity and text");

        auto other = warning;
        other.modId = "other";
        other.severity = IssueSeverity::Error;
        auto second = warning;
        second.id = "integration-unavailable";
        auto error = warning;
        error.id = "load-failed";
        error.severity = IssueSeverity::Error;
        check(registry.Report(other) && registry.Report(second) && registry.Report(error),
            "issue IDs are scoped to the mod and each mod can report multiple issues");

        const auto keys = [](const std::vector<ModIssue>& issues) {
            std::vector<std::string> result;
            for (const auto& entry : issues) result.push_back(entry.modId + "/" + entry.id);
            return result;
        };
        const std::vector<std::string> initialOrder{
            "other/missing-pack", "sample/load-failed", "sample/missing-pack", "sample/integration-unavailable"
        };
        const auto initial = registry.Snapshot();
        check(keys(initial) == initialOrder, "snapshots put errors first and preserve report order within each severity");
        check(registry.Report(warning) && keys(registry.Snapshot()) == initialOrder,
            "an identical report does not duplicate or reorder an issue");

        auto updated = warning;
        updated.severity = IssueSeverity::Error;
        updated.title = "Animation loading failed";
        updated.impact = "Scenes cannot start.";
        updated.nextSteps = "Reinstall the animation pack.\nRestart the game.";
        check(registry.Report(updated), "an existing issue can change severity and all its text");
        const auto changed = registry.Snapshot();
        check(keys(changed) == std::vector<std::string>{ "sample/missing-pack", "other/missing-pack", "sample/load-failed", "sample/integration-unavailable" },
            "severity changes retain the issue's original report position");
        check(changed[0].severity == updated.severity && changed[0].title == updated.title &&
            changed[0].impact == updated.impact && changed[0].nextSteps == updated.nextSteps,
            "updating an issue replaces its complete contents");
        check(initial[2].severity == warning.severity && initial[2].title == warning.title,
            "previous snapshots remain unchanged after registry updates");
        auto detached = changed;
        detached[0].title = "Snapshot-only edit";
        check(registry.Snapshot()[0].title == updated.title, "editing a snapshot does not edit the registry");
        check(registry.Report(warning) && keys(registry.Snapshot()) == initialOrder,
            "returning to warning severity restores the original warning order");

        check(!registry.Clear("unknown", "missing-pack") && !registry.Clear("sample", "unknown") &&
            keys(registry.Snapshot()) == initialOrder, "clearing unknown identities leaves all issues untouched");
        check(registry.Clear("sample", "missing-pack") && !registry.Clear("sample", "missing-pack"),
            "clearing removes an issue completely and repeating it is harmless");
        check(keys(registry.Snapshot()) == std::vector<std::string>{ "other/missing-pack", "sample/load-failed", "sample/integration-unavailable" },
            "clearing one mod's issue preserves the same ID in another mod");
        check(registry.Report(warning) && keys(registry.Snapshot()) == std::vector<std::string>{
            "other/missing-pack", "sample/load-failed", "sample/integration-unavailable", "sample/missing-pack" },
            "reporting a cleared issue starts a new position without retaining history");
        check(registry.ClearMod("sample") == 3 && keys(registry.Snapshot()) == std::vector<std::string>{ "other/missing-pack" },
            "clearing a mod removes all its issues and preserves other mods");
        check(registry.ClearMod("sample") == 0 && registry.ClearMod("unknown") == 0,
            "clearing a mod with no issues is harmless");
        check(registry.Clear("other", "missing-pack") && registry.Snapshot().empty(), "clearing the last issue leaves an empty snapshot");

        const ModIssue minimal{ .modId = "sample", .id = "missing-pack", .title = "Custom animations are unavailable" };
        check(registry.Report(minimal) && registry.Snapshot()[0].impact.empty() && registry.Snapshot()[0].nextSteps.empty(),
            "reports may omit both impact and next steps");
        auto impactOnly = minimal;
        impactOnly.impact = "Scenes use standard animations.";
        check(registry.Report(impactOnly) && registry.Snapshot()[0].impact == impactOnly.impact && registry.Snapshot()[0].nextSteps.empty(),
            "impact can be provided without next steps");
        auto stepsOnly = minimal;
        stepsOnly.nextSteps = "Install the animation pack.";
        check(registry.Report(stepsOnly) && registry.Snapshot()[0].impact.empty() && registry.Snapshot()[0].nextSteps == stepsOnly.nextSteps,
            "next steps can be provided without impact");
        check(registry.Report(warning) && registry.Report(minimal) && registry.Snapshot()[0].impact.empty() && registry.Snapshot()[0].nextSteps.empty(),
            "omitting optional fields on an update clears their previous contents");
        auto blankOptional = minimal;
        blankOptional.impact = " \t";
        blankOptional.nextSteps = "\r\n";
        check(registry.Report(blankOptional), "optional text may be blank");

        check(registry.Report(warning), "validation fixture is active");
        for (const auto* modId : { "", ".", "..", "Sample", "two words", "path/mod" }) {
            auto invalid = warning;
            invalid.modId = modId;
            check(!registry.Report(invalid), "invalid mod IDs are rejected");
        }
        for (auto field : { &ModIssue::modId, &ModIssue::id, &ModIssue::title }) {
            for (const auto& text : { std::string{}, std::string{ " \t\r\n" }, std::string("bad\0text", 8) }) {
                auto invalid = warning;
                invalid.*field = text;
                check(!registry.Report(invalid), "blank or NUL-containing required fields are rejected");
            }
        }
        for (auto field : { &ModIssue::impact, &ModIssue::nextSteps, &ModIssue::reason }) {
            auto invalid = warning;
            invalid.*field = std::string("bad\0text", 8);
            check(!registry.Report(invalid), "optional text must still be NUL-free in internal reports");
        }
        const auto unchanged = registry.Snapshot();
        check(unchanged.size() == 1 && unchanged[0].modId == warning.modId && unchanged[0].id == warning.id &&
            unchanged[0].severity == warning.severity && unchanged[0].title == warning.title &&
            unchanged[0].impact == warning.impact && unchanged[0].nextSteps == warning.nextSteps,
            "invalid reports neither add issues nor damage an existing issue");

        checks += TestDiagnosticsService();
        std::cout << checks << '/' << checks << " mod issue checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Mod issue test failed: " << error.what() << '\n';
        return 1;
    }
}
