#include "Diagnostics/ModIssue.h"
#include "Settings/SettingsSchema.h"

#include <iostream>
#include <stdexcept>

int main()
{
    using namespace OSFSettings;
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    try {
        ModIssue issue{ .mod = "sample", .id = "missing-pack" };
        check(IssueModName(issue, {}) == "sample", "a mod without settings uses its ID as its display name");

        std::vector<ModSettings> settings{
            { .schema = { .id = "other", .title = "Other Mod" } },
            { .schema = { .id = "sample", .title = "Sample Mod" } }
        };
        check(IssueModName(issue, settings) == "Sample Mod", "a matching schema supplies the display name");
        check(issue.mod == "sample", "resolving a display name preserves the issue's mod ID");

        const auto previousName = IssueModName(issue, settings);
        settings[1].schema.title = "Mód 日本語";
        check(IssueModName(issue, settings) == "Mód 日本語", "name lookup reflects the current schema title");
        check(previousName == "Sample Mod", "the returned name owns its text");

        settings[1].schema.title.clear();
        check(IssueModName(issue, settings) == "sample", "a schema with no title falls back to the mod ID");
        settings.pop_back();
        check(IssueModName(issue, settings) == "sample", "unrelated schemas do not supply a display name");

        std::cout << checks << '/' << checks << " mod issue checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Mod issue test failed: " << error.what() << '\n';
        return 1;
    }
}
