#pragma once

namespace OSFSettings
{
    // Refresh after schema/localization changes. Call outside the settings lock.
    void ReportCompatibilityIssues();
}
