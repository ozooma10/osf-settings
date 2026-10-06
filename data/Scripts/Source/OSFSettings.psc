ScriptName OSFSettings Native Hidden

; Installed plugin version = major * 10000 + minor * 100 + patch.
Int Function GetVersion() Global Native
Bool Function IsReady() Global Native

; Fallback is used on failure, including type mismatch and numeric overflow.
; IDs, keys and enum values follow the JSON schema's exact spelling.
Bool Function GetBool(String modId, String key, Bool fallback = false) Global Native
Int Function GetInt(String modId, String key, Int fallback = 0) Global Native
Float Function GetFloat(String modId, String key, Float fallback = 0.0) Global Native
String Function GetEnum(String modId, String key, String fallback = "") Global Native
String Function GetString(String modId, String key, String fallback = "") Global Native

; True means success, with the value already saved. Failures leave values unchanged.
Bool Function SetBool(String modId, String key, Bool value) Global Native
Bool Function SetInt(String modId, String key, Int value) Global Native
Bool Function SetFloat(String modId, String key, Float value) Global Native
Bool Function SetEnum(String modId, String key, String value) Global Native
Bool Function SetString(String modId, String key, String value) Global Native
Bool Function Reset(String modId, String key) Global Native
Bool Function ResetMod(String modId) Global Native

; Subscribe before reading. Callback: Function OnOSFSettingChanged(String modId, String key).
; Empty callback key means reread all settings (including the initial notification).
; receiver must be a bound script instance, e.g. a quest or reference alias.
; Static targets implement the same callback as a Global function.
; Registrations last for the session. True includes an already registered listener. Register again after each load;
Bool Function RegisterForChanges(ScriptObject receiver, String modId) Global Native
Bool Function RegisterForChangesStatic(String targetScript, String modId) Global Native

; Callback: Function OnOSFHotkey(String modId, String hotkeyId).
; The hotkey must be declared in the schema without a menu target.
Bool Function RegisterHotkey(ScriptObject receiver, String modId, String hotkeyId) Global Native
Bool Function RegisterHotkeyStatic(String targetScript, String modId, String hotkeyId) Global Native

; Exactly one handler per declared action. Repeating the same registration succeeds.
; Callback: Function OnOSFAction(String modId, String actionId, String invocation).
; Register again after load. Closing Settings does not retire actions.
Bool Function RegisterAction(ScriptObject receiver, String modId, String actionId) Global Native
Bool Function RegisterActionStatic(String targetScript, String modId, String actionId) Global Native
; Call once when finished, including immediate completion. Keep invocation as an opaque string. Stale/duplicate completion returns false.
Bool Function CompleteAction(String invocation, Bool succeeded, String message = "") Global Native

; Report current problems in Mod Issues. The same (modId, issueId) replaces the full report. Omitted details clear prior details.
; isError=false reports a warning; true reports an error. True means the report was accepted.
Bool Function ReportIssue(String modId, String issueId, String title, Bool isError = false, String impact = "", String nextSteps = "") Global Native
; Clearing an already absent issue succeeds. Reports last until cleared or the game exits.
Bool Function ClearIssue(String modId, String issueId) Global Native
Bool Function ClearModIssues(String modId) Global Native
