# Your first settings page

Ship a JSON schema and connect its values to your mod through C++ or Papyrus.
OSF Settings supplies the menu and persistence; a schema alone does not change
gameplay. Restart Starfield after changing schemas.

## 1. Define a setting

Place `mymod.json` under `Data/SFSE/Plugins/OSF/Settings/schemas` in your mod:

```json
{
  "schemaVersion": 1,
  "id": "mymod",
  "title": "My Mod",
  "description": "Configure My Mod.",
  "groups": {
    "General": [
      {
        "key": "enabled",
        "type": "bool",
        "label": "Enable feature",
        "hint": "Enable My Mod's feature.",
        "default": true
      }
    ]
  }
}
```

Choose a unique lowercase mod ID and stable setting keys. `groups` is an object
whose names are group labels and whose values are setting arrays. Additional
types are `int`, `float`, `enum`, `string` and `key`; see the [API](API.md) and
[Papyrus example schema](../examples/papyrus/papyrusexample.json).

## 2. Read and react

### Papyrus

Compile against the public `OSFSettings.psc`, supplied in `Scripts/Source`.
Attach this script to a player reference alias in your own running quest:

```papyrus
ScriptName MyModSettings extends ReferenceAlias

Event OnInit()
    InitializeSettings()
EndEvent

Event OnPlayerLoadGame()
    InitializeSettings()
EndEvent

Function InitializeSettings()
    If OSFSettings.IsReady()
        If OSFSettings.RegisterForChanges(Self, "mymod")
            ReadSettings()
        EndIf
    EndIf
EndFunction

Function ReadSettings()
    Bool enabled = OSFSettings.GetBool("mymod", "enabled", true)
    ; Apply enabled to your mod here.
EndFunction

Function OnOSFSettingChanged(String modId, String key)
    ReadSettings()
EndFunction
```

Subscribe before reading. An empty callback key requests a full refresh,
including the initial notification. Register again after loading a save;
registrations belong to that session. See the [complete examples](../examples/papyrus/README.md)
for all value types, hotkeys and Global callbacks, and [Papyrus](PAPYRUS.md) for
lifecycle and failure handling. Include your compiled consumer PEX and quest in
your mod; depend on OSF Settings rather than bundling another provider DLL/PEX.

### Native C++

Include `sdk/OSFSettings.h` with your CommonLibSF-based plugin. During SFSE
`kPostPostLoad`, initialize a client for an initial read:

```cpp
OSFSettings::API::Client settings;
if (settings.Init() && settings.IsReady()) {
    bool enabled = true;
    if (settings.GetBool("mymod", "enabled", &enabled) == OSFSettings::API::Status::Ok) {
        // Apply or retain enabled in the context your mod requires.
    }
}
```

This snippet is a one-time read. Add `Subscribe` for live changes; keep the client
and callback context alive and balance the subscription with `Unsubscribe`.
Callbacks invalidate values: reread them, and treat a null key as a full refresh.
They do not guarantee the engine's main thread. The [API](API.md)
defines those rules; the [registry consumer](../examples/registry/README.md)
demonstrates complete subscription, refresh and cleanup ownership.

## 3. Add a hotkey when needed

Add a top-level declaration beside `groups`:

```json
"hotkeys": [
  { "id": "toggle", "label": "Toggle feature", "default": "F6" }
]
```

Register `toggle` through `OSFSettings.RegisterHotkey` and `OnOSFHotkey` in
Papyrus, or `Client::RegisterHotkey` in C++. The framework owns native rebinding
and Controls persistence; your callback performs the action. Omit `default` to
start unbound. See [native hotkey examples](../examples/hotkeys/README.md).
An ordinary `type: "key"` setting stores a key value; use `hotkeys` for native
action dispatch. Settings values persist across saves, rather than per character.
