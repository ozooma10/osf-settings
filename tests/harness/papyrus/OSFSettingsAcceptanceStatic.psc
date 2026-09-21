ScriptName OSFSettingsAcceptanceStatic Hidden

Function InitializeSettings() Global
    Bool changes = OSFSettings.RegisterForChangesStatic("OSFSettingsAcceptanceStatic", "osfacceptance")
    Bool hotkey = OSFSettings.RegisterHotkeyStatic("OSFSettingsAcceptanceStatic", "osfacceptance", "pulse")
    OSFSettingsAcceptanceProbe.Record("static", "registered", "", "", changes && hotkey && OSFSettings.IsReady())
EndFunction

Function OnOSFSettingChanged(String modId, String settingKey) Global
    OSFSettingsAcceptanceProbe.Record("static", "changed", settingKey, OSFSettings.GetString(modId, "caption"), modId == "osfacceptance")
EndFunction

Function OnOSFHotkey(String modId, String hotkeyId) Global
    OSFSettingsAcceptanceProbe.Record("static", "hotkey", hotkeyId, "", modId == "osfacceptance" && hotkeyId == "pulse")
EndFunction

Function WriteValues() Global
    ; The instance writes; this observer only acknowledges actual VM execution.
    OSFSettingsAcceptanceProbe.Record("static", "written", "", "", true)
EndFunction

Function ReadValues() Global
    Bool ok = OSFSettings.GetBool("osfacceptance", "enabled") && OSFSettings.GetInt("osfacceptance", "count") == 7
    ok = OSFSettings.GetFloat("osfacceptance", "volume") > 0.64 && ok
    ok = OSFSettings.GetEnum("osfacceptance", "mode") == "quiet" && ok
    OSFSettingsAcceptanceProbe.Record("static", "read", "caption", OSFSettings.GetString("osfacceptance", "caption"), ok)
EndFunction
Function InspectFixture() Global
    Quest fixture = Game.GetFormFromFile(0x800, "OSFSettingsAcceptance.esm") as Quest
    ReferenceAlias playerAlias = fixture.GetAlias(0) as ReferenceAlias
    If !fixture.IsRunning()
        fixture.Start()
    EndIf
    If playerAlias.GetReference() != Game.GetPlayer()
        playerAlias.ForceRefTo(Game.GetPlayer())
    EndIf
    OSFSettingsAcceptanceProbe.Record("fixture", "lookup", fixture.GetFormID() as String, (fixture as String) + " / " + (playerAlias as String) + " / " + (playerAlias.GetReference() as String), fixture != None)
    OSFSettingsAcceptanceProbe.Record("fixture", "alias", "", fixture.IsRunning() as String, playerAlias.GetReference() == Game.GetPlayer())
EndFunction
