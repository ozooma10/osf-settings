ScriptName OSFSettingsExampleGlobal Hidden

; A Global script has no OnInit. An owning quest/alias must call this after each load.
Function InitializeSettings() Global
    Bool changes = OSFSettings.RegisterForChangesStatic("OSFSettingsExampleGlobal", "papyrusexample")
    Bool hotkey = OSFSettings.RegisterHotkeyStatic("OSFSettingsExampleGlobal", "papyrusexample", "toggle")
    Bool registeredAction = OSFSettings.RegisterActionStatic("OSFSettingsExampleGlobal", "papyrusexample", "delayed")
    Debug.Trace("OSF Settings Global example: action registered = " + registeredAction)
    Debug.Trace("OSF Settings Global example: registrations " + changes + ", " + hotkey)
EndFunction

Function OnOSFSettingChanged(String asModId, String asKey) Global
    Debug.Trace("OSF Settings Global example: changed " + asModId + "/" + asKey)
EndFunction

Function OnOSFHotkey(String modId, String hotkeyId) Global
    ; Only the instance example toggles the setting; this observer just records delivery.
    Debug.Trace("OSF Settings Global example: hotkey " + modId + "/" + hotkeyId)
EndFunction

Function OnOSFAction(String modId, String actionId, String invocation) Global
    ; Starfield's menu-aware wait can progress while Settings pauses the game.
    Utility.WaitMenuPause(2.0)
    Bool completed = OSFSettings.CompleteAction(invocation, true, "The delayed Global action completed.")
    Debug.Trace("OSF Settings Global example: completion accepted = " + completed)
EndFunction
