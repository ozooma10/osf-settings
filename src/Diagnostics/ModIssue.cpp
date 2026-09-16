#include "ModIssue.h"
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
}
