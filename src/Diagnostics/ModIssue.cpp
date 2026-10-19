#include "ModIssue.h"
#include "Settings/Localization.h"
#include "Settings/SettingsSchema.h"

#include <algorithm>
#include <map>

namespace OSFSettings
{
    namespace
    {
        // Invalid display text must not prevent a compatibility warning from being reported.
        std::string DisplayName(const ModSchema& schema)
        {
            return IsValidString(schema.title, 256) && !schema.title.empty() ? schema.title : schema.id;
        }
    }

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

    std::vector<ModIssue> DependencyIssues(std::span<const ModSettings> settings)
    {
        std::map<std::string, std::vector<std::pair<std::string, SettingsVersion>>> consumers;
        for (const auto& mod : settings) {
            for (const auto& [id, minimum] : mod.schema.dependencies) {
                consumers[id].emplace_back(DisplayName(mod.schema), minimum);
            }
        }

        std::vector<ModIssue> issues;
        for (const auto& [id, wanted] : consumers) {
            const auto provider = std::ranges::find(settings, id, [](const ModSettings& mod) -> const std::string& { return mod.schema.id; });
            const bool installed = provider != settings.end();
            // An unversioned manifest cannot be compared, so stay quiet rather than guess.
            if (installed && !provider->schema.version) continue;
            std::optional<SettingsVersion> highest;
            std::string affected;
            for (const auto& [name, minimum] : wanted) {
                if (installed && minimum <= *provider->schema.version) continue;
                highest = std::max(highest.value_or(minimum), minimum);
                if (!affected.empty()) affected += '\n';
                affected += tr("issues.dependencyConsumer", {{ "mod", name }, { "version", minimum.String() }});
            }
            if (!highest) continue;
            const auto name = installed ? DisplayName(provider->schema) : id;
            issues.push_back({
                .modId = "osfsettings",
                .id = "dependency:" + id,
                .severity = IssueSeverity::Warning,
                .title = tr(installed ? "issues.dependencyUpdate" : "issues.dependencyMissing", {{ "mod", name }}),
                .impact = (installed ?
                    tr("issues.dependencyUpdateImpact", {{ "mod", name }, { "installed", provider->schema.version->String() }}) :
                    tr("issues.dependencyMissingImpact", {{ "mod", id }})) + "\n\n" + affected,
                .nextSteps = tr("issues.dependencyNextSteps", {{ "mod", name }, { "version", highest->String() }}),
                .nexusModId = installed ? provider->schema.nexusModId : 0,
                .installedVersion = installed ? provider->schema.version->String() : "",
                .requiredVersion = highest->String()
            });
        }
        return issues;
    }
}
