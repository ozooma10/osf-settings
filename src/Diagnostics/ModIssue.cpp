#include "ModIssue.h"
#include "Settings/Localization.h"
#include "Settings/SettingsSchema.h"

#include <algorithm>
#include <format>
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

    std::string IssueReport(const ModIssue& issue, std::span<const ModSettings> settings, std::string_view pluginVersion)
    {
        // Labels stay English so mod authors can read reports from any language.
        std::string report = "OSF Settings issue report\n";
        const auto field = [&](std::string_view label, const std::string& value) {
            if (value.empty()) return;
            report += std::format("{}: ", label);
            // Indent continuation lines so multi-paragraph text stays under its label.
            for (const char c : value) {
                report += c;
                if (c == '\n') report += "  ";
            }
            report += '\n';
        };
        std::string affected;
        for (const auto& name : issue.affectedMods) affected += (affected.empty() ? "" : ", ") + name;
        const auto reporter = IssueModName(issue, settings);
        field("Title", issue.title);
        field("Severity", issue.severity == IssueSeverity::Error ? "Error" : "Warning");
        field("Affects", affected.empty() ? reporter : affected);
        field("Reported by", reporter == issue.modId ? reporter : std::format("{} ({})", reporter, issue.modId));
        field("Issue ID", std::format("{}/{}", issue.modId, issue.id));
        field("Impact", issue.impact);
        field("To fix", issue.nextSteps);
        if (!issue.requiredVersion.empty()) {
            field("Installed version", issue.installedVersion.empty() ? "not installed" : issue.installedVersion);
            field("Required version", issue.requiredVersion);
        }
        field("Reason", issue.reason);
        field("Note", issue.note);
        if (issue.nexusModId) field("Mod page", std::format("https://www.nexusmods.com/starfield/mods/{}", issue.nexusModId));
        field("OSF Settings version", std::string(pluginVersion));
        return report;
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
                .reason = error.message,
                .note = tr("issues.schemaFailedNote"),
                // The file failed to parse, so its title is unknown.
                .affectedMods = { tr("issues.schemaOwner", {{ "file", file }}) }
            });
        }
        return issues;
    }

    std::vector<ModIssue> DependencyIssues(std::span<const ModSettings> settings, std::span<const SettingsLoadError> errors)
    {
        std::map<std::string, std::vector<std::pair<std::string, SettingsVersion>>> consumers;
        for (const auto& mod : settings) {
            for (const auto& [id, minimum] : mod.schema.dependencies) {
                consumers[id].emplace_back(DisplayName(mod.schema), minimum);
            }
        }
        for (const auto& error : errors) {
            if (!error.schema) continue;
            const auto name = error.file.filename().u8string();
            const std::string file(reinterpret_cast<const char*>(name.data()), name.size());
            for (const auto& [id, minimum] : error.dependencies) {
                consumers[id].emplace_back(file, minimum);
            }
        }

        std::vector<ModIssue> issues;
        for (const auto& [id, wanted] : consumers) {
            const auto provider = std::ranges::find(settings, id, [](const ModSettings& mod) -> const std::string& { return mod.schema.id; });
            const bool installed = provider != settings.end();
            // An unversioned manifest cannot be compared, so stay quiet rather than guess.
            if (installed && !provider->schema.version) continue;
            std::optional<SettingsVersion> highest;
            std::vector<std::string> affected;
            for (const auto& [name, minimum] : wanted) {
                if (installed && minimum <= *provider->schema.version) continue;
                highest = std::max(highest.value_or(minimum), minimum);
                affected.push_back(name);
            }
            if (!highest) continue;
            const auto name = installed ? DisplayName(provider->schema) : id;
            issues.push_back({
                .modId = "osfsettings",
                .id = "dependency:" + id,
                .severity = IssueSeverity::Warning,
                .title = tr(installed ? "issues.dependencyUpdate" : "issues.dependencyMissing", {{ "mod", name }}),
                .impact = tr(installed ? "issues.dependencyUpdateImpact" : "issues.dependencyMissingImpact", {{ "mod", name }}),
                .nextSteps = tr(installed ? "issues.dependencyUpdateSteps" : "issues.dependencyMissingSteps",
                    {{ "mod", name }, { "version", highest->String() }}),
                .nexusModId = installed ? provider->schema.nexusModId : 0,
                .reason = installed ? "" : tr("issues.dependencyMissingReason", {{ "mod", id }}),
                .note = tr("issues.dependencyNote"),
                .affectedMods = std::move(affected),
                .installedVersion = installed ? provider->schema.version->String() : "",
                .requiredVersion = highest->String()
            });
        }
        return issues;
    }

    std::vector<ModIssue> ExtensionIssues(std::span<const ModSettings> settings)
    {
        std::vector<ModIssue> issues;
        for (const auto& mod : settings) {
            const auto& schema = mod.schema;
            if (schema.extends.empty() || schema.page != schema.id) continue;
            const auto host = std::ranges::find(settings, schema.extends,
                [](const ModSettings& other) -> const std::string& { return other.schema.id; });
            // A host that is not installed is already reported as a missing dependency.
            if (host == settings.end()) continue;
            const auto name = DisplayName(schema);
            issues.push_back({
                .modId = "osfsettings",
                .id = "extension:" + schema.id,
                .severity = IssueSeverity::Warning,
                .title = tr("issues.extensionUnresolved", {{ "mod", name }}),
                .impact = tr("issues.extensionUnresolvedImpact", {{ "host", DisplayName(host->schema) }}),
                .nextSteps = tr("issues.extensionUnresolvedSteps", {{ "mod", name }}),
                .nexusModId = schema.nexusModId,
                .reason = tr("issues.extensionUnresolvedReason", {{ "mod", schema.id }}),
                .note = tr("issues.dependencyNote"),
                .affectedMods = { name }
            });
        }
        return issues;
    }
}
