# Persistence

**Status:** Hosted by OSF Settings; originally implemented in OSF Animation 1.2.0. Public ABI: `OSFPersistenceAPI` 1.0. Disk format: `OSFP` 1.

## Settings and UI state

Every mod keeps its own `Data/SFSE/Plugins/OSF/Settings/values/<modId>.json`.
These values apply across game saves. OSF Settings uses `values/osfsettings.json`
for its own `values` and a separate `launcher` section containing recent views.
Updating history preserves OSF Settings' values, and updating those values preserves
history. Other mods' files are independent. Schemas and translations remain authored
files; ship schemas, not player values.

Hotkey bindings, including OSF Settings' menu hotkey, remain in Starfield's
`ControlMap_Custom.txt` and use the native bindings menu.

If `osfsettings.json` has no launcher section, OSF reads the old
`launcher-history.json`. The next successful OSF value or history change saves it
alongside OSF's values. The old history file is retained and ignored once the new
section exists. Existing per-mod values files require no migration.

Successful setting writes reach disk before values and notifications are published.
History failures retain session recency and retry on a later launch. Unchanged history
is not written again. A mutex serializes updates to OSF's shared file; temporary writes,
file-buffer flushes, and atomic replacement preserve the previous file on failure.
An unreadable or unsupported OSF values file blocks changes to that file until it is
repaired and the game restarted; other mods can still save their settings.

Consumers continue to use the typed settings API. Per-save native records use the
independent ABI below.

## Purpose and ownership

OSF Settings owns one `.osf` sidecar beside each Starfield save and brokers isolated record streams for native SFSE plugins. A client owns its own record types, versions, and migration logic. The broker does not understand client payloads, expose the physical file cursor, patch SFSE's `SerializationInterface`, or use the `.sfse` extension.

Animation retains `OSF_RequestPersistenceAPI` as a forwarding export for existing consumers.
It returns the Settings-owned table and installs no persistence hooks. Without a compatible,
ready `OSFSettings.dll`, per-save persistence is unavailable; Animation retains local scene
teardown. New consumers should resolve the factory directly from OSF Settings. Upgrade the
host and Animation together; an older Animation release still attempts to own these hooks.

The copyable consumer header is [`sdk/OSFPersistenceAPI.h`](../sdk/OSFPersistenceAPI.h). The only exported symbol is:

```cpp
extern "C" __declspec(dllexport)
const OSFPersistenceAPI* OSF_RequestPersistenceAPI(uint32_t requestedVersion);
```

The ABI contains only fixed-width integers, opaque pointers, and C function pointers. Each public structure starts with `size`. OSF owns the returned table and the writer/reader callback tables; consumers never delete or retain the per-callback writer/reader.

## ABI stability

`OSF_PERSISTENCE_API_VERSION` packs `MAJOR << 16 | MINOR`. A major change may break layout or semantics. Within one major, additions are append-only at the end of a structure and increment MINOR. Existing fields are not reordered or retyped. A consumer initializes `size` to the structure size it compiled against and requests the version it understands.

The 1.0 factory accepts requests for major 1 with a requested minor no newer than the host. It returns `nullptr` for an incompatible version. `GetInterfaceVersion` reports the table version; `GetPluginVersion` reports the OSF Settings semantic version.

## Acquisition and registration

Consumers dynamically resolve the factory and do not link OSF Settings:

```cpp
#include "OSFPersistenceAPI.h"
#include <Windows.h>

const OSFPersistenceAPI* g_persistence{};

bool RegisterPersistence()
{
    HMODULE module = GetModuleHandleW(L"OSFSettings.dll");
    if (!module) return false;

    auto request = reinterpret_cast<OSF_RequestPersistenceAPI_t>(
        GetProcAddress(module, "OSF_RequestPersistenceAPI"));
    if (!request) return false;

    const OSFPersistenceAPI* api = request(OSF_PERSISTENCE_API_VERSION);
    if (!api || api->size < sizeof(OSFPersistenceAPI) || !api->IsReady(api)) return false;

    static MyState state;
    OSFPersistenceClientV1 client{};
    client.size = sizeof(client);
    client.clientID = OSF_PERSISTENCE_FOURCC('D', 'E', 'M', 'O');
    client.clientName = "Example Plugin";
    client.context = &state;
    client.save = SaveState;
    client.load = LoadState;
    client.revert = RevertState;
    client.formDeleted = FormDeleted;

    if (!api->RegisterClient(api, &client)) return false;
    g_persistence = api;
    return true;
}
```

Register from the SFSE `kPostDataLoad` message. OSF installs and validates its hooks during its plugin load, before that message, so plugin load order does not affect registration at `kPostDataLoad`. A request made earlier may find no module or a table whose `IsReady` is false; retry at `kPostDataLoad`.

The host copies the descriptor fields and `clientName`, but not `context`; the context and callback code must remain alive until `UnregisterClient(api, clientID, context)` returns. Registration and unregistration are thread-safe. Client IDs are unique process-wide; collisions and duplicate registrations fail and are logged. Unregistration removes the client from future snapshots and waits for an already-running callback, except that a client may safely unregister itself from its current callback.

## Client IDs and record versions

Use a stable, printable FourCC assigned to one plugin or subsystem. Published reservations are:

- `OSFP`: physical container magic; never a client ID.
- `OSFA`: OSF Animation's internal client.
- `OSFD`: reserved for OSF Defeat.
- `OSFO`: reserved for OSF Outpost Siege.

Record types are client-local FourCCs. They need not be globally unique because the client block is the namespace. Increment a record's `version` when its payload schema changes. Load callbacks should switch on both type and version, read formats they support, and ignore unknown types or future versions. Ignoring a record requires no seek: call `GetNextRecordInfo` again and the host discards the unread tail.

A minimal callback pair:

```cpp
static uint8_t OSF_PERSISTENCE_CALL SaveState(void* opaque, OSFRecordWriterV1* writer)
{
    auto& state = *static_cast<MyState*>(opaque);
    if (!writer->OpenRecord(writer, OSF_PERSISTENCE_FOURCC('D','A','T','0'), 1)) return 0;
    return writer->WriteRecordData(writer, &state.disk, sizeof(state.disk));
}

static uint8_t OSF_PERSISTENCE_CALL LoadState(void* opaque, OSFRecordReaderV1* reader)
{
    auto& state = *static_cast<MyState*>(opaque);
    uint32_t type{}, version{}, length{};
    while (reader->GetNextRecordInfo(reader, &type, &version, &length)) {
        if (type != OSF_PERSISTENCE_FOURCC('D','A','T','0') ||
            version != 1 || length != sizeof(state.disk)) {
            continue;
        }
        if (reader->ReadRecordData(reader, &state.disk, sizeof(state.disk)) != sizeof(state.disk)) return 0;
    }
    return 1;
}
```

Return zero from `save` or `load` on failure. A save failure omits that client's entire staged block while other clients continue. A load failure is logged while later clients continue. The contract forbids exceptions crossing the ABI; OSF catches unexpected C++ exceptions defensively at every client boundary.

A save callback that opens no records produces no client block.

## Threading and lifecycle

All persistence callbacks are serialized with one another, but the caller is not guaranteed to be the game thread. Do not call or mutate `RE::*` game state directly in a callback. Decode into consumer-owned plain data, use `ResolveFormID` while loading, then enqueue engine-state application on SFSE's game-task queue.

For a world-replacing load, OSF performs:

1. Snapshot registrations and invoke every `revert` callback.
2. Open and validate the matching `.osf` sidecar.
3. Skip unknown or corrupt client blocks.
4. Invoke `load` only for registered clients with a valid block.

A missing sidecar is a clean load after revert; it does not invoke `load`. Save invokes every registered client's `save`. Form deletion events invoke every registered `formDeleted` callback. Callback invocation never holds the registry mutex.

The load reader bounds every read to the current record and client block. Calling `GetNextRecordInfo` skips any unread bytes from the previous record. `ResolveFormID` applies remap/delete information captured for the current load; a false result means the form no longer resolves. ABI 1.0 has no `ResolveHandle`, because the current host cannot promise correct cross-plugin handle remapping. It can be appended in a future minor once that support is reliable.

OSF Animation registers itself as a consumer through the same broker as `OSFA`. Its current internal persistence responsibility is the pre-load scene/graph teardown previously performed by `SaveSafety`; there is no privileged parallel record path.

## Sidecar lifecycle and format

For a save named `Example.sfs`, the sidecar is `Example.osf` in the configured Starfield save directory. The save, load, and delete hooks use the engine-provided name, covering manual/named saves, save-as, quicksave/quickload, autosave, and reload paths that pass those common engine functions. A successful Starfield delete also deletes its matching sidecar.

The sidecar does **not** automatically participate in Steam/Xbox cloud sync. A cloud-restored `.sfs` without its `.osf` loads as a clean state after revert. Users or sync tools must copy both files if persistent mod state must travel with a save.

All fields are unsigned little-endian 32-bit values:

```text
FileHeader (24 bytes)
  magic = 'OSFP'
  formatVersion = 1
  headerSize
  OSF host version
  clientCount
  flags/reserved

ClientHeader (20 bytes)
  clientID
  recordCount
  payloadLength
  CRC-32 of the complete record payload
  reserved

RecordHeader (12 bytes), repeated inside the bounded client payload
  type
  version
  payloadLength
  payload bytes
```

Clients are written in ascending numeric client-ID order. Limits are 4,096 client blocks, 65,536 records per client, 16 MiB per record, 64 MiB per client payload, and 512 MiB per sidecar. Client payload lengths preserve the boundary to a following block; CRC or record-stream failure therefore skips one block without shifting later clients. An untrustworthy truncated outer header/payload ends parsing because no safe next boundary exists.

Saves stage every client in memory, then write `<save>.osf.tmp`, flush the C stream, commit the OS file buffer, close it, and atomically replace the old `.osf` with write-through semantics. If staging, writing, flushing, closing, or replacement fails, the previous `.osf` remains in place.
