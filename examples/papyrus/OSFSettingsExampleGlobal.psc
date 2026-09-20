ScriptName OSFSettingsExampleGlobal Hidden

; A Global script has no OnInit. An owning quest/alias must call this after each load.
Function InitializeSettings() Global
    Bool changes = OSFSettings.RegisterForChangesStatic("OSFSettingsExampleGlobal", "papyrusexample")
    Bool hotkey = OSFSettings.RegisterHotkeyStatic("OSFSettingsExampleGlobal", "papyrusexample", "toggle")
    Debug.Trace("OSF Settings Global example: registrations " + changes + ", " + hotkey)
EndFunction

Function OnOSFSettingChanged(String asModId, String asKey) Global
    Debug.Trace("OSF Settings Global example: changed " + asModId + "/" + asKey)
EndFunction

Function OnOSFHotkey(String modId, String hotkeyId) Global
    ; Only the instance example toggles the setting; this observer just records delivery.
    Debug.Trace("OSF Settings Global example: hotkey " + modId + "/" + hotkeyId)
EndFunction
