# Registry consumer example

This development-only SFSE plugin discovers loaded mods and keeps an owned copy
of their current typed settings. It includes only the public SDK and CommonLibSF;
it does not include SettingsStore, SettingsService, or any OSF UI code.
Registry consumers include `OSFSettingsRegistry.h`, which also includes the core
`OSFSettings.h` API.

```powershell
xmake build osfsettings-registry-example
```

The target is excluded from default builds and installation. For manual use,
place `OSFSettingsRegistryExample.dll` under `Data/SFSE/Plugins` beside the current
OSF Settings provider. It needs no example schema: it discovers the installed
schemas. Startup logs show mod, group, and setting identities and setting types.
Do not ship the example with the provider.

`RegistryConsumer::Start()` performs discovery at `kPostPostLoad`, subscribes to
each discovered mod, then reads again to populate its cache. Discovery values
are deliberately not published because they precede subscription. Schemas are
fixed after startup, so this covers the complete mod set for the process.

The callback copies every string by byte length and every scalar by its public
type. `Snapshot()` returns a consumer-owned map, including mods with no values.
Enum and string values use the same local string storage but retain their
distinct setting types. Keys retain their native numeric codes. Public metadata
can be copied the same way; the logging callback demonstrates its enumeration.

Both initial notifications and whole-mod resets have a null changed key. The
example handles these explicitly by reading the affected mod. Keyed
notifications also refresh that mod, keeping the example small. Notifications
are invalidations, not a history of writes: reread current values, and tolerate
duplicates. Never use a null key as a setting identity or send it as an OSF UI
`settings.changed` event.

Acquisition and cache replacement share one consumer mutex, so an older capture
cannot overwrite a newer refresh. Provider callbacks hold no provider lock.
The cache does not touch engine/UI state; effects must be scheduled in whatever
context their engine API requires. `LastRefresh()` exposes the last notification
refresh result. A failed refresh preserves the last good values; a real bridge
must retain the invalidation and retry before claiming its cache is current.

Call `Start()` and `Stop()` serially from the owner, outside callbacks. On failure,
Start releases subscriptions it already acquired. Stop calls Unsubscribe without
holding the refresh mutex, because unsubscribe can wait for a callback using
that mutex. Keep the owner alive until Stop returns. The destructor calls Stop;
the SFSE example deliberately keeps a successful owner alive until process exit
instead of invoking plugin APIs from DLL teardown.

Host checks exercise this same consumer in `osfsettings-registry-tests`. They
establish API and cache behavior, not in-game OSF UI integration. See the
[registry contract](../../docs/API.md#registry-discovery-and-snapshots) for the
complete metadata and lifetime rules.
