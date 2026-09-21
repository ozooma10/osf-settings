ScriptName OSFSettingsAcceptanceInstance extends ObjectReference

Function InitializeSettings()
    Bool changes = OSFSettings.RegisterForChanges(Self, "osfacceptance")
    Bool hotkey = OSFSettings.RegisterHotkey(Self, "osfacceptance", "pulse")
    OSFSettingsAcceptanceProbe.Record("instance", "registered", "", "", changes && hotkey && OSFSettings.IsReady())
EndFunction

Function OnOSFSettingChanged(String modId, String settingKey)
    OSFSettingsAcceptanceProbe.Record("instance", "changed", settingKey, OSFSettings.GetString(modId, "caption"), modId == "osfacceptance")
EndFunction

Function OnOSFHotkey(String modId, String hotkeyId)
    OSFSettingsAcceptanceProbe.Record("instance", "hotkey", hotkeyId, "", modId == "osfacceptance" && hotkeyId == "pulse")
EndFunction

Function WriteValues()
    Bool ok = OSFSettings.SetBool("osfacceptance", "enabled", true)
    ok = OSFSettings.SetInt("osfacceptance", "count", 7) && ok
    ok = OSFSettings.SetFloat("osfacceptance", "volume", 0.65) && ok
    ok = OSFSettings.SetEnum("osfacceptance", "mode", "quiet") && ok
    ok = OSFSettings.SetString("osfacceptance", "caption", "MiXeD Café") && ok
    OSFSettingsAcceptanceProbe.Record("instance", "written", "", "", ok)
EndFunction

Function ReadValues()
    Bool ok = OSFSettings.GetBool("osfacceptance", "enabled") && OSFSettings.GetInt("osfacceptance", "count") == 7
    ok = OSFSettings.GetFloat("osfacceptance", "volume") > 0.64 && ok
    ok = OSFSettings.GetEnum("osfacceptance", "mode") == "quiet" && ok
    OSFSettingsAcceptanceProbe.Record("instance", "read", "caption", OSFSettings.GetString("osfacceptance", "caption"), ok)
EndFunction
Function InspectFixture()
EndFunction
