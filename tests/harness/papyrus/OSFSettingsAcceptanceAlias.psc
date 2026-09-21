ScriptName OSFSettingsAcceptanceAlias extends ReferenceAlias

Event OnInit()
    InitializeSettings("init")
EndEvent

Event OnPlayerLoadGame()
    InitializeSettings("load")
EndEvent

Function InitializeSettings(String reason)
    Bool ok = OSFSettings.IsReady()
    ok = OSFSettings.RegisterForChanges(Self, "osfacceptance") && ok
    ok = OSFSettings.RegisterHotkey(Self, "osfacceptance", "pulse") && ok
    OSFSettingsAcceptanceProbe.Record("alias", "registered", reason, "", ok)
EndFunction

Function OnOSFSettingChanged(String modId, String settingKey)
    OSFSettingsAcceptanceProbe.Record("alias", "changed", settingKey, OSFSettings.GetString(modId, "caption"), modId == "osfacceptance")
EndFunction

Function OnOSFHotkey(String modId, String hotkeyId)
    OSFSettingsAcceptanceProbe.Record("alias", "hotkey", hotkeyId, "", modId == "osfacceptance" && hotkeyId == "pulse")
EndFunction
