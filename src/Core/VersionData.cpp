#include "SFSE/SFSE.h"

SFSE_PLUGIN_VERSION = []() noexcept {
    SFSE::PluginVersionData version{};
    version.PluginVersion({ 1, 0, 0 });
    version.PluginName("OSF Settings Slim");
    version.AuthorName("ozooma10");
    version.UsesSigScanning(false);
    version.UsesAddressLibrary(true);
    version.HasNoStructUse(false);
    version.IsLayoutDependent(true);
    version.CompatibleVersions({ SFSE::RUNTIME_SF_1_16_244 });
    return version;
}();
