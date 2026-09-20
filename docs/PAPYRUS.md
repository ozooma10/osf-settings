# Papyrus

OSF Settings ships the native hidden script
[OSFSettings.psc](../data/Scripts/Source/OSFSettings.psc) and its compiled PEX.
It uses the same JSON schemas, values files, validation, menu, and native
hotkeys as the C++ API. The framework needs no ESM. The
[consumer example](../examples/papyrus/README.md) includes both instance and
Global callbacks.

## Values

Declare settings in Data/SFSE/Plugins/OSF/Settings/schemas/<modId>.json.
Use the exact authored mod ID, key, and enum option ID. Script type names are
case-insensitive; settings identifiers and enum values retain the service's
exact matching rules.

~~~papyrus
If OSFSettings.IsReady()
    Bool enabled = OSFSettings.GetBool("mymod", "enabled", false)
    Int count = OSFSettings.GetInt("mymod", "count", 3)
    Float volume = OSFSettings.GetFloat("mymod", "volume", 0.5)
    String mode = OSFSettings.GetEnum("mymod", "mode", "normal")
    String caption = OSFSettings.GetString("mymod", "caption", "")
    Bool saved = OSFSettings.SetBool("mymod", "enabled", !enabled)
EndIf
~~~

GetVersion returns major * 10000 + minor * 100 + patch (10000 for this release).
IsReady checks service readiness and whether a world transition has suspended
the bridge.

Each getter returns its fallback on error. SetBool, SetInt, SetFloat, SetEnum,
SetString, Reset(modId, key), and ResetMod(modId) return true on success.
Successful writes are already saved before return; equal values are successful
no-ops. A failed write preserves the previous value and generates no notification.
Failures log the function, setting identity, and reason to OSFSettings.log.
Settings are shared across save games; loading a save does not roll them back.

Papyrus Int is signed 32-bit. An int64 setting outside that range returns the
fallback, without clamping or wrapping. Papyrus Float uses single precision:
ordinary rounding, including underflow to zero, is allowed; nonfinite values
and overflow return the fallback. Writes widen Papyrus values into the service
and use the schema's existing validation.

Enums and free-form strings are distinct types. Neither getter/setter accepts
the other type. Strings retain the existing UTF-8 byte limits and text policy.
The adapter adds no trimming or normalization and uses case-sensitive string
pool entries when returning text. Actual Papyrus VM casing and Unicode round
trips still require in-game acceptance.

## Change notifications

~~~papyrus
Bool registered = OSFSettings.RegisterForChanges(Self, "mymod")
; Subscribe before the first read, so changes cannot fall between those operations.
Bool enabled = OSFSettings.GetBool("mymod", "enabled", false)

Function OnOSFSettingChanged(String asModId, String asKey)
    ; Empty asKey requests a full refresh, including the initial notification.
    ; Reread the values you use. Notifications may coalesce.
EndFunction
~~~

The receiver must be a bound script instance, such as a quest, reference, or
reference alias. Registrations use weak handle/type identities and do not keep
the receiver alive. Unavailable receivers are skipped and logged.

RegisterForChangesStatic("MyGlobalScript", "mymod") invokes the same function
declared Global on that script. Both forms validate the target identity; authors
must provide the named callback with exactly two String parameters and no return
value. A missing callback or incorrect signature is reported by VM dispatch.

## Hotkeys

Declare a top-level schema hotkey without a menu target, then register:

~~~papyrus
Bool registered = OSFSettings.RegisterHotkey(Self, "mymod", "toggle")

Function OnOSFHotkey(String asModId, String asHotkeyId)
    ; Apply your mod's action here.
EndFunction
~~~

RegisterHotkeyStatic("MyGlobalScript", "mymod", "toggle") invokes a Global
OnOSFHotkey with the same arguments. There is no initial hotkey notification.
Fresh key-down activates once; repeats and releases do not activate callbacks.
Native input admission and OSF hotkey blocks apply to Papyrus as well. Menu
targets cannot also register a Papyrus callback.

The bridge submits calls directly when the settings service notifies a listener
or a hotkey press is admitted. It adds no delivery queue or polling step.
Papyrus schedules the accepted script call; submission does not make the script
body synchronous or guarantee a particular frame. Instance calls use the VM's
handle-and-script dispatch overload.

## Registration lifetime

All registration functions return true on success, or false on failure.
Repeating the same receiver, kind, mod, and hotkey succeeds without adding a
callback or repeating the initial change notification.

Registrations last for the loaded session and are cleaned up automatically.
Register from OnInit and again from a player alias's OnPlayerLoadGame. A Global
script has no initialization event: an owning quest or alias must call its
registration function after each load. To temporarily disable a feature, its
callback can check whether the feature is enabled before doing any work.

Ordinary saves preserve registrations. A load/exit begin suspends submission and
ignores new hotkey activations. A refused or cancelled load resumes the surviving
session and submits a full refresh for each listener whose settings changed while
suspended. Calls already submitted to Papyrus cannot be recalled. TESLoadGameEvent
clears the outgoing registrations. Opening MainMenu also clears them, covering
quit-to-menu followed by new game. No script object pointer is retained across
these boundaries.

Native functions bind separately, immediately after the engine's own binding pass
during GameVM construction. The hook calls the previous target first so other
plugins using the same call site can chain. It is verified for Starfield
1.16.244.0 and disabled on other versions or an unexpected call instruction.
Save/load and menu events only manage subscriptions; they do not rebind natives.

## Build and acceptance

The DLL build compiles the public script into build/papyrus/OSFSettings.pex.
Installation places it in Scripts and its source in Scripts/Source. Examples
are compiled on request and never installed automatically.

~~~powershell
pwsh tools/build-papyrus.ps1 -Examples
xmake build osfsettings-papyrus-tests
xmake run osfsettings-papyrus-tests
xmake run osfsettings-hotkey-callback-tests
xmake build "OSF Settings"
xmake install "OSF Settings"
~~~

The helper accepts -Compiler and -Imports for alternate CK/compiler locations.
Native host checks cover number conversions, fallback behavior, type separation,
persistence failures, initial/coalesced notifications, registration identity,
direct submission, session cleanup during delivery, and transition cancellation.
These checks exercise the bridge core; they do not execute a Papyrus VM.

When in-game testing is requested, use SettingsSmoke through the OSF Test
Harness with a player-alias consumer fixture. Acceptance must cover startup,
native binding before script initialization, menu-to-script and script-to-menu
changes, callback signatures, string casing/Unicode, hotkeys/rebinding/blocks,
ordinary saves, repeated and cancelled loads, and quit-to-menu/new game.
VM submission from the settings and input callbacks, lifecycle event ordering,
and receiver resolution remain unverified until that fresh-game test passes.
