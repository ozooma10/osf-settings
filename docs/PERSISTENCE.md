# Persistence

## Settings and UI state

Every mod keeps its own `Data/SFSE/Plugins/OSF/Settings/values/<modId>.json`.
These values apply across game saves, including OSF Settings' own
`values/osfsettings.json`. Schemas and translations remain authored files; ship
schemas, not player values.

OSF's internal launcher recency uses `Data/SFSE/Plugins/OSF/Settings/internal.json`,
with a fixed `recentLaunchers` array of `mod`/`id` pairs. It is independent of mod
settings and is shared across saves.

Hotkey bindings, including OSF Settings' menu hotkey, remain in Starfield's
`ControlMap_Custom.txt` and use the native bindings menu.

Successful setting writes complete before values and notifications are published.
History failures retain session recency and retry on a later launch. Unchanged history
is not written again. Invalid internal data starts with empty history. Temporary
writes are closed and checked before replacement, preserving the previous file on failure.

Consumers continue to use the typed settings API.
