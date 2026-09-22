#requires -Version 7.2
[CmdletBinding()]
param(
    [string]$HarnessRoot = 'C:\Modding\Starfield\OSF Test Harness',
    [ValidateSet('en','de','ja')][string[]]$Language = @('en','de','ja'),
    [switch]$LargeText
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
. (Join-Path $HarnessRoot 'scripts\Common.ps1')
. (Join-Path $HarnessRoot 'scripts\Environment.ps1')
. (Join-Path $HarnessRoot 'scripts\Reports.ps1')
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $HarnessRoot 'scenarios') -Filter '*.ps1' -File) { . $file.FullName }
$script:Config.SettingsProject = $repo
$Scenario = 'SettingsSmoke'
$Acceptance = $false
$AcceptancePhase = 'All'
$NativeBindings = $false
$TranslationRegistration = $true
$backups = @{}
$lock = $null

function Backup-LocalizationFile([string]$Path) {
    if (-not $backups.ContainsKey($Path)) {
        $backups[$Path] = if (Test-Path -LiteralPath $Path) { [IO.File]::ReadAllBytes($Path) } else { $null }
    }
}

function Close-LocalizationMO2 {
    if (@(Get-GameProcess).Count) { return }
    $exe = Join-Path $script:Config.MO2 'ModOrganizer.exe'
    foreach ($mo in @(Get-Process -Name ModOrganizer -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe })) {
        $null = $mo.CloseMainWindow()
        if (-not $mo.WaitForExit(10000)) { throw '[blocked] Idle MO2 could not close normally before restoring the test profile.' }
    }
}

try {
    $lock = [IO.File]::Open((Join-Path $script:StateRoot 'runner.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $null = Get-OwnedGame
    Stop-TestGame
    Close-LocalizationMO2
    foreach ($name in @('Starfield.ini','StarfieldCustom.ini','StarfieldPrefs.ini','modlist.txt','settings.ini','plugins.txt','loadorder.txt')) {
        Backup-LocalizationFile (Assert-ChildPath $script:ProfileRoot (Join-Path $script:ProfileRoot $name))
    }
    $staging = Join-Path $HarnessRoot 'staging\OSF Settings'
    $destination = Assert-ChildPath $script:ModRoot (Join-Path $script:ModRoot 'OSF Testing - Settings')
    foreach ($locale in $Language) {
        $TranslationRegistration = $true
        $null = Start-RunArtifacts "SettingsLocalization-$locale-$(if ($LargeText) { 'large' } else { 'normal' })"
        Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $script:Run.path 'Test-Localization.ps1')
        $outcome = 'passed'
        $message = "Localization $locale completed."
        try {
            Initialize-Harness
            $modlist = Join-Path $script:ProfileRoot 'modlist.txt'
            $enabled = @('OSF Testing Output','OSF Testing - Settings','OSF Test Harness','OSF Testing - Auto Load')
            $disabled = @('OSF Settings','OSF Settings Slim','OSFSettings')
            $lines = foreach ($line in Get-Content -LiteralPath $modlist) {
                if ($line.Length -gt 1 -and $line[0] -in @('+','-') -and $line.Substring(1) -in $enabled) { '+' + $line.Substring(1) }
                elseif ($line.Length -gt 1 -and $line[0] -in @('+','-') -and $line.Substring(1) -in $disabled) { '-' + $line.Substring(1) }
                else { $line }
            }
            [IO.File]::WriteAllLines($modlist, [string[]]$lines, [Text.UTF8Encoding]::new($false))
            Copy-Tree $staging $destination
            Prepare-SettingsFixtures
            foreach ($name in @('Starfield.ini','StarfieldCustom.ini')) {
                Set-IniValue (Join-Path $script:ProfileRoot $name) 'General' 'sLanguage' $locale
            }
            $base = Join-Path $destination 'SFSE\Plugins\OSF\Settings'
            $schemaFile = Assert-ChildPath $destination (Join-Path $base 'schemas\localization-example.json')
            Backup-LocalizationFile $schemaFile
            Copy-Item -LiteralPath (Join-Path $repo 'examples\localization\schemas\localization-example.json') -Destination $schemaFile -Force
            $expected = @{ heading = 'MOD SETTINGS'; title = 'Localization example'; setting = 'Mode'; native = 'Mod Bindings'; action = 'OSF Settings: Open mod settings' }
            if ($locale -ne 'en') {
                $coreFile = Assert-ChildPath $destination (Join-Path $base "l10n\$locale\osfsettings.json")
                $modFile = Assert-ChildPath $destination (Join-Path $base "l10n\$locale\localization-example.json")
                Backup-LocalizationFile $coreFile
                Backup-LocalizationFile $modFile
                $core = @{version=1;ui=@{};hotkeys=@{openMenu=@{label=''}}}
                $mod = Get-Content -LiteralPath (Join-Path $repo 'examples\localization\l10n\de\localization-example.json') -Raw | ConvertFrom-Json -AsHashtable
                if ($locale -eq 'de') {
                    $expected = @{heading='MOD-EINSTELLUNGEN';title='Lokalisierungsbeispiel';setting='Modus';native='Mod-Tastenbelegung';action='OSF Settings: Mod-Einstellungen öffnen'}
                    $core.ui = @{'menu.title'=$expected.heading;'bindings.heading'=$expected.native;'menu.autoSave'='Änderungen werden automatisch gespeichert.';'buttons.runAction'='AKTION AUSFÜHREN';'buttons.cancel'='ABBRECHEN';'counts.mods'='Mods: {count}'}
                    $core.hotkeys.openMenu.label = 'Mod-Einstellungen öffnen'
                } else {
                    $expected = @{heading='MOD設定';title='日本語の設定例';setting='モード';native='MOD キー割り当て';action='OSF Settings: MOD設定を開く'}
                    $core.ui = @{'menu.title'=$expected.heading;'bindings.heading'=$expected.native;'menu.autoSave'='変更は自動的に保存されます。';'buttons.runAction'='実行';'buttons.cancel'='キャンセル';'counts.mods'='MOD数: {count}'}
                    $core.hotkeys.openMenu.label = 'MOD設定を開く'
                    $mod.title=$expected.title; $mod.description='元のスキーマを変更せずに表示テキストを翻訳します。'
                    $mod.groups.General.label='一般'
                    $mod.settings.enabled=@{label='機能を有効にする';hint='この機能のオンとオフを切り替えます。'}
                    $mod.settings.mode=@{label=$expected.setting;hint='表示する情報量を選択してください。';optionLabels=@{quiet='静か';normal='通常'}}
                    $mod.hotkeys.open.label='サンプルメニューを開く'
                    $mod.actions.reset=@{label='状態をリセット';hint='このMODの状態をリセットします。';confirmation='本当にリセットしますか？'}
                }
                Write-JsonFile $coreFile $core
                Write-JsonFile $modFile $mod
                Copy-Item -LiteralPath $coreFile -Destination (Join-Path $script:Run.path 'osfsettings-catalog.json')
                Copy-Item -LiteralPath $modFile -Destination (Join-Path $script:Run.path 'mod-catalog.json')
            }
            Add-Evidence 'configuration' 'Game language and separately installed translation catalog' @{language=$locale;largeText=[bool]$LargeText;schemaSha256=(Get-FileHash $schemaFile).Hash}
            $null = Start-TestGame
            # The ordinary helper's translation assertion expects English; this scenario checks the selected catalog below.
            $TranslationRegistration = $false
            $state = Open-Settings
            Assert-Test ($state.localization.language -ceq $locale -and $state.localization.errors -eq 0) 'Detected actual game language and loaded catalogs' $state.localization
            Assert-Test ($state.ui.largeText -eq [bool]$LargeText) 'Engine selected the requested text variant' $state.ui.largeText
            Assert-Test ($state.ui.menuHeading -ceq $expected.heading) 'Movie received localized OSF interface text' $state.ui.menuHeading
            Assert-Test ($state.translationRegistration.passed -and $state.translationRegistration.native.passed) 'Constructor and native translation checks passed' $state.translationRegistration
            Assert-Test ($state.translationRegistration.native.heading -ceq $expected.native -and $state.ui.translations.heading -ceq $expected.native) 'Native and movie Controls heading match the catalog' $state.ui.translations
            Assert-Test ($state.translationRegistration.native.action -ceq $expected.action -and $state.ui.translations.originalContext -ceq $expected.action) 'Localized hotkey and conflict tokens match' $state.translationRegistration.native
            $null = Save-Capture 'localized-root'
            $state = Select-SettingsRow 'localization-example'
            Assert-Test ($state.ui.selection.title -ceq $expected.title) 'Unchanged third-party schema title is translated' $state.ui.selection
            Send-SettingsAction 'Accept'
            $null = Wait-Observed 'Localized example opened' { Get-SettingsState } { param($s) $s.ui.mod -eq 'localization-example' -and -not $s.ui.refreshing } 10
            $state = Select-SettingsRow 'localization-example' 'mode'
            Assert-Test ($state.ui.selection.title -ceq $expected.setting) 'Setting label uses the drop-in catalog' $state.ui.selection
            $null = Save-Capture 'localized-mod'
            Send-SettingsAction 'Accept'
            $state = Wait-Observed 'Enum saves its original value' { Get-SettingsState } { param($s) $s.ui.selection.key -eq 'mode' -and $s.ui.selection.value -eq 'normal' } 8
            Assert-Test ($state.ui.selection.value -ceq 'normal') 'Translated enum choice saves the original identifier' $state.ui.selection
            Write-JsonFile (Join-Path $script:Run.path 'localization-state.json') $state
            Close-Settings
            $state = Open-Settings
            Assert-Test ($state.ui.menuHeading -ceq $expected.heading) 'Localization survives closing and reopening' $state.ui.menuHeading
            Close-Settings
        } catch {
            $outcome = if ($_.Exception.Message.StartsWith('[failed]')) { 'failed' } else { 'blocked' }
            $message = $_.Exception.Message
            try { if (Get-OwnedGame) { $null = Save-Capture 'failure' } } catch { Write-Warning $_ }
            throw
        } finally {
            try { Stop-TestGame } catch { Write-Warning $_ }
            finally { Finish-RunArtifacts $outcome $message }
        }
    }
} finally {
    try { if ($lock) { Stop-TestGame; Close-LocalizationMO2 } } catch { Write-Warning $_ } finally {
        foreach ($entry in $backups.GetEnumerator()) {
            if ($null -eq $entry.Value) { if (Test-Path -LiteralPath $entry.Key) { Remove-Item -LiteralPath $entry.Key } }
            else { [IO.File]::WriteAllBytes($entry.Key, $entry.Value) }
        }
        if ($lock) { $lock.Dispose() }
    }
}
