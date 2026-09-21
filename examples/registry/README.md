# Registry example

Discovers loaded mods, copies their values, and refreshes a cache on changes.
[RegistryConsumer.h](RegistryConsumer.h) handles subscriptions and cleanup using
only the public SDK. See the [registry guide](../../docs/REGISTRY.md).

Build from the repository root with the Windows/MSVC toolchain:

```powershell
xmake build osfsettings-registry-example
```

For testing, copy `OSFSettingsRegistryExample.dll` to `Data/SFSE/Plugins` alongside
OSF Settings. No example schema is needed. Startup logs list the discovered settings.
The example is excluded from normal builds and installation.

Call `Start()` and `Stop()` serially, outside callbacks. Keep the owner alive until
`Stop()` returns. `Snapshot()` returns owned values. Check `LastRefresh()` for
refresh failures; the example retains its last good cache but does not retry automatically.
