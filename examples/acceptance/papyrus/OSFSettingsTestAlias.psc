ScriptName OSFSettingsTestAlias extends ReferenceAlias

Event OnInit()
    InitializeSettings()
EndEvent

Event OnPlayerLoadGame()
    InitializeSettings()
EndEvent

Function InitializeSettings()
    Bool ok = OSFSettings.RegisterForChanges(Self, "osfsettings-test")
    ok = OSFSettings.RegisterHotkey(Self, "osfsettings-test", "papyrusAlias") && ok
    ok = OSFSettings.RegisterAction(Self, "osfsettings-test", "papyrusAlias") && ok
    ok = OSFSettings.RegisterAction(Self, "osfsettings-test", "papyrusValues") && ok
    ok = OSFSettings.RegisterAction(Self, "osfsettings-test", "papyrusReset") && ok
    ok = OSFSettings.RegisterAction(Self, "osfsettings-test", "papyrusIssue") && ok
    ok = OSFSettings.RegisterAction(Self, "osfsettings-test", "papyrusClear") && ok
    OSFSettings.SetString("osfsettings-test", "papyrusStatus", "Ready: alias registrations = " + ok)
    Debug.Trace("OSF Settings Test Mod: alias registrations = " + ok)
    OSFSettingsTestGlobal.InitializeSettings()
EndFunction

Function OnOSFSettingChanged(String modId, String settingKey)
    ; Status writes must never recursively update themselves.
    If settingKey == "enabled" || settingKey == "count" || settingKey == "amount" || settingKey == "mode" || settingKey == "text" || settingKey == ""
        OSFSettings.SetString(modId, "papyrusStatus", "Alias change: " + settingKey + "; count=" + OSFSettings.GetInt(modId, "count"))
        Debug.Trace("OSF Settings Test Mod: alias changed " + settingKey)
    EndIf
EndFunction

Function OnOSFHotkey(String modId, String hotkeyId)
    Int hits = OSFSettings.GetInt(modId, "papyrusAliasHits") + 1
    OSFSettings.SetInt(modId, "papyrusAliasHits", hits)
    OSFSettings.SetString(modId, "papyrusStatus", "Alias hotkey count=" + hits)
    Debug.Notification("OSF Test: Papyrus alias hotkey #" + hits)
EndFunction

Function OnOSFAction(String modId, String actionId, String invocation)
    Bool ok = True
    String resultText = "Papyrus alias action completed."
    If actionId == "papyrusValues"
        ok = OSFSettings.SetBool(modId, "enabled", False)
        ok = OSFSettings.SetInt(modId, "count", 7) && ok
        ok = OSFSettings.SetFloat(modId, "amount", 0.65) && ok
        ok = OSFSettings.SetEnum(modId, "mode", "quiet") && ok
        ok = OSFSettings.SetString(modId, "text", "Papyrus typed values") && ok
        ok = !OSFSettings.GetBool(modId, "enabled", True) && ok
        ok = (OSFSettings.GetInt(modId, "count") == 7) && ok
        ok = (OSFSettings.GetFloat(modId, "amount") > 0.64) && ok
        ok = (OSFSettings.GetFloat(modId, "amount") < 0.66) && ok
        ok = (OSFSettings.GetEnum(modId, "mode") == "quiet") && ok
        ok = (OSFSettings.GetString(modId, "text") == "Papyrus typed values") && ok
        resultText = "Papyrus typed read/write passed=" + ok
    ElseIf actionId == "papyrusReset"
        ok = OSFSettings.Reset(modId, "enabled")
        ok = OSFSettings.GetBool(modId, "enabled", False) && ok
        resultText = "Papyrus reset to true passed=" + ok
    ElseIf actionId == "papyrusIssue"
        ok = OSFSettings.ReportIssue(modId, "papyrus", "EXPECTED TEST: Papyrus warning", False, "No gameplay problem; tests script reporting.", "Use Papyrus clear issue.")
        resultText = "Papyrus issue reported=" + ok
    ElseIf actionId == "papyrusClear"
        ok = OSFSettings.ClearIssue(modId, "papyrus")
        resultText = "Papyrus issue cleared=" + ok
    EndIf
    OSFSettings.SetString(modId, "papyrusStatus", resultText)
    Bool completed = OSFSettings.CompleteAction(invocation, ok, resultText)
    Debug.Trace("OSF Settings Test Mod: alias action " + actionId + " completed=" + completed)
EndFunction
