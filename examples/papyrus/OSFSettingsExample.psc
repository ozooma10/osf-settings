ScriptName OSFSettingsExample extends ReferenceAlias

Event OnInit()
    InitializeSettings()
EndEvent

Event OnPlayerLoadGame()
    InitializeSettings()
EndEvent

Function InitializeSettings()
    If !OSFSettings.IsReady()
        Debug.Trace("OSF Settings example: provider unavailable")
        Return
    EndIf
    Bool changes = OSFSettings.RegisterForChanges(Self, "papyrusexample")
    Bool hotkey = OSFSettings.RegisterHotkey(Self, "papyrusexample", "toggle")
    Bool registeredAction = OSFSettings.RegisterAction(Self, "papyrusexample", "reset")
    Debug.Trace("OSF Settings example: action registered = " + registeredAction)
    Debug.Trace("OSF Settings example: registrations " + changes + ", " + hotkey)
    OSFSettingsExampleGlobal.InitializeSettings()
    ReadSettings()
EndFunction

Function ReadSettings()
    Bool enabled = OSFSettings.GetBool("papyrusexample", "enabled", false)
    Int count = OSFSettings.GetInt("papyrusexample", "count", 3)
    Float volume = OSFSettings.GetFloat("papyrusexample", "volume", 0.5)
    String mode = OSFSettings.GetEnum("papyrusexample", "mode", "normal")
    String caption = OSFSettings.GetString("papyrusexample", "caption", "Hello")
    Debug.Trace("OSF Settings example: " + enabled + ", " + count + ", " + volume + ", " + mode + ", " + caption)
EndFunction

Function OnOSFSettingChanged(String modId, String key)
    ; Empty key is a full refresh. Reading all five small values is also fine for individual changes.
    ReadSettings()
EndFunction

Function OnOSFHotkey(String modId, String hotkeyId)
    If hotkeyId == "toggle"
        Bool enabled = OSFSettings.GetBool(modId, "enabled", false)
        If !OSFSettings.SetBool(modId, "enabled", !enabled)
            Debug.Trace("OSF Settings example: could not save toggle")
        EndIf
    EndIf
EndFunction

Function WriteExampleValues()
    Bool saved = OSFSettings.SetInt("papyrusexample", "count", 4)
    saved = OSFSettings.SetFloat("papyrusexample", "volume", 0.75) && saved
    saved = OSFSettings.SetEnum("papyrusexample", "mode", "quiet") && saved
    saved = OSFSettings.SetString("papyrusexample", "caption", "Updated") && saved
    Debug.Trace("OSF Settings example: writes saved = " + saved)
EndFunction

Function ResetExampleValues()
    Bool saved = OSFSettings.Reset("papyrusexample", "caption")
    saved = OSFSettings.ResetMod("papyrusexample") && saved
    Debug.Trace("OSF Settings example: resets saved = " + saved)
EndFunction

Function OnOSFAction(String modId, String actionId, String invocation)
    Bool saved = OSFSettings.ResetMod(modId)
    String resultText = "Example settings reset."
    If !saved
        resultText = "Could not save the example defaults."
    EndIf
    Bool completed = OSFSettings.CompleteAction(invocation, saved, resultText)
EndFunction
