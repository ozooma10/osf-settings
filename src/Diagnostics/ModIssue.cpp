#include "ModIssue.h"
#include "Settings/Localization.h"
#include "Settings/SettingsSchema.h"

namespace OSFSettings
{
    std::string IssueModName(const ModIssue& issue, std::span<const ModSettings> settings)
    {
        for (const auto& mod : settings) {
            if (mod.schema.id == issue.modId) {
                return mod.schema.title.empty() ? issue.modId : mod.schema.title;
            }
        }
        return issue.modId;
    }

    std::vector<ModIssue> SchemaLoadIssues(std::span<const SettingsLoadError> errors)
    {
        std::vector<ModIssue> issues;
        for (const auto& error : errors) {
            if (!error.schema) continue;
            const auto name = error.file.filename().u8string();
            const std::string file(reinterpret_cast<const char*>(name.data()), name.size());
            issues.push_back({
                .modId = "osfsettings",
                .id = "schema:" + file,
                .severity = IssueSeverity::Error,
                .title = tr("issues.schemaFailed", {{ "file", file }}),
                .impact = tr("issues.schemaFailedImpact"),
                .nextSteps = tr("issues.schemaFailedNextSteps"),
                .reason = error.message
            });
        }
        return issues;
    }

    std::optional<ModIssue> SettingsUpdateIssue(std::span<const ModSettings> settings,
        std::span<const SettingsLoadError> errors, const SettingsVersion& installed)
    {
        auto highest = installed;
        std::string affected;
        const auto add = [&](std::string_view name, const std::optional<SettingsVersion>& expected) {
            if (!expected || *expected <= installed) return;
            if (*expected > highest) highest = *expected;
            if (!affected.empty()) affected += '\n';
            affected += tr("issues.settingsUpdateMod", {{ "mod", name }, { "version", expected->String() }});
        };
        for (const auto& mod : settings) {
            // Invalid display text must not prevent the compatibility warning from being reported.
            add(IsValidString(mod.schema.title, 256) && !mod.schema.title.empty() ? mod.schema.title : mod.schema.id,
                mod.schema.expectedSettingsVersion);
        }
        for (const auto& error : errors) {
            if (!error.schema) continue;
            const auto name = error.file.filename().u8string();
            add(std::string(reinterpret_cast<const char*>(name.data()), name.size()), error.expectedSettingsVersion);
        }
        if (highest == installed) return std::nullopt;
        return ModIssue{
            .modId = "osfsettings",
            .id = "settings-update-recommended",
            .severity = IssueSeverity::Warning,
            .title = tr("issues.settingsUpdate"),
            .impact = tr("issues.settingsUpdateImpact", {{ "installed", installed.String() },
                { "expected", highest.String() }, { "mods", affected }}),
            .nextSteps = tr("issues.settingsUpdateNextSteps", {{ "version", highest.String() }})
        };
    }
}
