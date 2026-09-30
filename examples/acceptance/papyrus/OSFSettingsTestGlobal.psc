ScriptName OSFSettingsTestGlobal Hidden

Function Bootstrap() Global
    Quest fixture = Game.GetFormFromFile(0x800, "OSFSettingsTestMod.esm") as Quest
    If fixture == None
        OSFSettings.SetString("osfsettings-test", "papyrusStatus", "ERROR: enable OSFSettingsTestMod.esm and restart")
        Debug.Notification("OSF Test: fixture ESM is missing")
        Return
    EndIf
    If !fixture.IsRunning()
        fixture.Start()
    EndIf
    OSFSettingsTestAlias playerAlias = fixture.GetAlias(0) as OSFSettingsTestAlias
    If playerAlias == None
        OSFSettings.SetString("osfsettings-test", "papyrusStatus", "ERROR: test alias script missing")
        Return
    EndIf
    If playerAlias.GetReference() != Game.GetPlayer()
        playerAlias.ForceRefTo(Game.GetPlayer())
    EndIf
    playerAlias.InitializeSettings()
    Debug.Notification("OSF Test: Papyrus initialized; check status rows")
EndFunction

Function InitializeSettings() Global
    Bool ok = OSFSettings.RegisterForChangesStatic("OSFSettingsTestGlobal", "osfsettings-test")
    ok = OSFSettings.RegisterHotkeyStatic("OSFSettingsTestGlobal", "osfsettings-test", "papyrusGlobal") && ok
    ok = OSFSettings.RegisterActionStatic("OSFSettingsTestGlobal", "osfsettings-test", "papyrusGlobal") && ok
    OSFSettings.SetString("osfsettings-test", "papyrusGlobalStatus", "Ready: Global registrations = " + ok)
    Debug.Trace("OSF Settings Test Mod: Global registrations = " + ok)
EndFunction

Function OnOSFSettingChanged(String modId, String settingKey) Global
    If settingKey == "enabled" || settingKey == "count" || settingKey == "amount" || settingKey == "mode" || settingKey == "text" || settingKey == ""
        OSFSettings.SetString(modId, "papyrusGlobalStatus", "Global change: " + settingKey + "; count=" + OSFSettings.GetInt(modId, "count"))
        Debug.Trace("OSF Settings Test Mod: Global changed " + settingKey)
    EndIf
EndFunction

Function OnOSFHotkey(String modId, String hotkeyId) Global
    Int hits = OSFSettings.GetInt(modId, "papyrusGlobalHits") + 1
    OSFSettings.SetInt(modId, "papyrusGlobalHits", hits)
    OSFSettings.SetString(modId, "papyrusGlobalStatus", "Global hotkey count=" + hits)
    Debug.Notification("OSF Test: Papyrus Global hotkey #" + hits)
EndFunction

Function OnOSFAction(String modId, String actionId, String invocation) Global
    Utility.WaitMenuPause(3.0)
    String resultText = "Papyrus Global action finished after 3 seconds."
    OSFSettings.SetString(modId, "papyrusGlobalStatus", resultText)
    Bool completed = OSFSettings.CompleteAction(invocation, True, resultText)
    Debug.Trace("OSF Settings Test Mod: Global action completed=" + completed)
EndFunction
