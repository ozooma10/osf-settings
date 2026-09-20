# Papyrus consumer example

This development-only example uses every value type, writes/resets, change
subscriptions, and a schema hotkey. It demonstrates callbacks on a player
reference alias and on a Global script.

Compile the public API and both example scripts:

~~~powershell
pwsh tools/build-papyrus.ps1 -Examples
~~~

The API PEX is installed by the normal OSF Settings build. Example PEX files
land in build/papyrus/examples and are excluded from framework installation.

For a consumer mod:

1. Ship papyrusexample.json under Data/SFSE/Plugins/OSF/Settings/schemas.
2. Ship both example PEX files under Data/Scripts.
3. Attach OSFSettingsExample to a player reference alias in your own running
   quest. Its OnInit and OnPlayerLoadGame register both instance and Global
   callbacks before reading values.
4. Bind "Toggle example" in Settings. It starts unbound. Each fresh press toggles
   Enabled through the instance callback; the Global observer only logs delivery.

No example ESM is supplied or installed. Global functions do not initialize
themselves; OSFSettingsExampleGlobal.InitializeSettings is called by the alias.
For Global-only use, your existing initialization/load script can call it instead.

WriteExampleValues and ResetExampleValues demonstrate the remaining setters
and resets. Registration returns Bool; repeated calls do not add duplicate
listeners. Instance and Global registrations are cleaned up automatically when
the session ends. Register again after each load.

See the [API and lifecycle contract](../../docs/PAPYRUS.md).
Compilation is host proof only; the owning quest, load timing, and callbacks
need the SettingsSmoke in-game acceptance fixture before claiming runtime proof.
