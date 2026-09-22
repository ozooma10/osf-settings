#pragma once

namespace OSFSettings::Persistence::PersistenceHost
{
    // Owns the OSFP sidecar lifecycle. No dependency on animation or global settings.
    bool Initialize();
    void RegisterEventSinks();
}
