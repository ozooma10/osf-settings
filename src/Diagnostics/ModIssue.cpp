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
}
