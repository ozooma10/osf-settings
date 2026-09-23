#requires -Version 7.2
[CmdletBinding()]
param([string]$Harness = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'OSF Test Harness'))
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $repo 'packaging/ReleaseValidation.ps1')
$scratch = Join-Path $repo ('build/release-selftest/' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($scratch) | Out-Null
$script:checks = 0
function Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Self-test failed: $Message" }
    $script:checks++; Write-Host "PASS $Message"
}
function Reject([scriptblock]$Operation, [string]$Pattern) {
    $errorText = ''
    try { & $Operation | Out-Null } catch { $errorText = $_.Exception.Message }
    Check ($errorText -like "*$Pattern*") "Reject $Pattern"
}
function New-Candidate([string]$Name, [string]$Fault = '') {
    $directory = Join-Path $scratch $Name
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $archive = Join-Path $directory 'candidate.zip'
    $zip = [IO.Compression.ZipFile]::Open($archive, 'Create')
    $records = @()
    try {
        $paths = @(Get-ReleasePayloadPaths)
        if ($Fault -eq 'traversal') { $paths[-1] = '../escaped.dll' }
        if ($Fault -eq 'duplicate') { $paths[-1] = $paths[0] }
        foreach ($path in $paths) {
            $payload = if ($path.EndsWith('.dll')) { 'MZ OSFSettings_RequestAPI OSFSettings_RequestLauncherAPI OSFSettings_RequestDiagnosticsAPI' }
                elseif ($path.EndsWith('.swf')) { 'FWS production fixture' }
                elseif ($path.EndsWith('.json')) { '{}' } else { 'fixture' }
            if ($Fault -eq 'test-dll' -and $path.EndsWith('.dll')) { $payload += ' OSFSettings_TestSnapshot' }
            if ($Fault -eq 'test-movie' -and $path.EndsWith('.swf')) { $payload += ' reportTestState' }
            [byte[]]$bytes = if ($path.EndsWith('.pex')) { [byte[]](0xDE,0xC0,0x57,0xFA,0) } else { [Text.Encoding]::UTF8.GetBytes($payload) }
            if ($Fault -eq 'bad-pex' -and $path.EndsWith('.pex')) { $bytes[0] = 0 }
            $entry = $zip.CreateEntry($path)
            $stream = $entry.Open()
            try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
            $records += @{ path=$path; bytes=$bytes.Length; sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)) }
        }
    } finally { $zip.Dispose() }
    if ($Fault -eq 'file-hash') { $records[0].sha256 = '0' * 64 }
    $hash = (Get-FileHash -LiteralPath $archive).Hash
    "$hash  candidate.zip" | Set-Content -LiteralPath "$archive.sha256"
    $manifest = Join-Path $directory 'candidate.manifest.json'
    @{ archive='candidate.zip'; sha256=$hash; configuration='windows/x64/releasedbg; test_harness=n'; files=$records } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifest
    return $manifest
}
$good = New-Candidate 'valid'
$candidate = Test-ReleaseArchive $good
Check ($candidate.files -eq 16) 'Valid archive has all expected payloads'
$preserved = Test-ReleaseReinstall $candidate.archive (Join-Path $scratch 'reinstall')
Check ($preserved.Count -eq 3) 'Reinstall preserves all player-state sentinels'
foreach ($fault in @(
    @('traversal','Unexpected or duplicate'), @('duplicate','Invalid payload record'),
    @('test-dll','Instrumented DLL'), @('test-movie','Development movie'),
    @('bad-pex','PEX header'), @('file-hash','Payload checksum')
)) {
    $manifest = New-Candidate $fault[0] $fault[0]
    Reject { Test-ReleaseArchive $manifest } $fault[1]
}
Add-Content -LiteralPath $candidate.archive -Value 'tampered'
Reject { Test-ReleaseArchive $good } 'archive checksum'

# Exercise the actual suite's orchestration in a disposable fake harness. The
# child runner below only writes evidence; no game, MO2, input or XMake is used.
$fake = Join-Path $scratch 'harness'
foreach ($directory in @('scripts','scenarios','native','state','artifacts','settings/packaging','build/bin','staging/OSF Settings','mods/OSF Testing - Settings','mods/OSF Test Harness','profiles/OSF Testing','overwrite/SFSE/Plugins')) {
    [IO.Directory]::CreateDirectory((Join-Path $fake $directory)) | Out-Null
}
Copy-Item -LiteralPath (Join-Path $Harness 'Test-SettingsRelease.ps1') -Destination $fake
@('+OSF Testing - Settings', '+OSF Test Harness') | Set-Content -LiteralPath (Join-Path $fake 'profiles/OSF Testing/modlist.txt')
@'
Set-StrictMode -Version Latest
$script:Config = @{ SettingsProject=(Join-Path (Split-Path $PSScriptRoot -Parent) 'settings') }
$script:Config.MO2 = Split-Path $PSScriptRoot -Parent
$script:Config.TestProfile = 'OSF Testing'
$script:StateRoot = Join-Path (Split-Path $PSScriptRoot -Parent) 'state'
$script:ArtifactRoot = Join-Path (Split-Path $PSScriptRoot -Parent) 'artifacts'
$script:ModRoot = Join-Path (Split-Path $PSScriptRoot -Parent) 'mods'
function Write-JsonFile([string]$Path, $Value) { $Value | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $Path }
function Read-JsonFile([string]$Path) { if (Test-Path -LiteralPath $Path) { Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -AsHashtable } }
'@ | Set-Content -LiteralPath (Join-Path $fake 'scripts/Common.ps1')
@'
function Get-ReleaseSourceIdentity([string]$Repository) { return @{sha256='fixture-source'; algorithm='self-test'} }
'@ | Set-Content -LiteralPath (Join-Path $fake 'settings/packaging/ReleaseValidation.ps1')
@'
param([string]$Action, [string]$Scenario, [switch]$SkipBuild, [string]$ResultPath,
    [switch]$Acceptance, [string]$AcceptancePhase, [switch]$LargeText, [switch]$Keybindings,
    [switch]$TextInput, [switch]$TranslationRegistration, [switch]$NativeBindings)
if ($Action -eq 'Build') {
    if ($env:OSF_RELEASE_SELFTEST_MODE -eq 'build-fail') { exit 2 }
    $files = @('SFSE/Plugins/OSFSettings.dll','Interface/OSFSettingsMenu.swf','Interface/OSFSettingsMenu_LRG.swf','Scripts/OSFSettings.pex',
        'Scripts/OSFSettingsAcceptanceInstance.pex','Scripts/OSFSettingsAcceptanceStatic.pex','Scripts/OSFSettingsAcceptanceProbe.pex','Scripts/OSFSettingsAcceptanceAlias.pex','OSFSettingsAcceptance.esm')
    foreach ($base in @('staging/OSF Settings','mods/OSF Testing - Settings')) {
        foreach ($file in $files) {
            $path = Join-Path $PSScriptRoot "$base/$file"
            [IO.Directory]::CreateDirectory((Split-Path $path -Parent)) | Out-Null
            [IO.File]::WriteAllText($path, 'fake binary')
        }
    }
    foreach ($file in @('OSFTestHarness.dll','OSFTestInput.exe','OSFTestOcr.exe')) { [IO.File]::WriteAllText((Join-Path $PSScriptRoot "build/bin/$file"), 'fake binary') }
    exit 0
}
if ($env:OSF_RELEASE_SELFTEST_MODE -eq 'no-receipt') { exit 0 }
$started = [DateTime]::UtcNow.ToString('o')
$path = Join-Path $PSScriptRoot ('artifacts/fake-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($path) | Out-Null
$code = if ($env:OSF_RELEASE_SELFTEST_MODE -eq 'blocked') { 2 } elseif ($env:OSF_RELEASE_SELFTEST_MODE -eq 'failed') { 1 } else { 0 }
$passed = $code -eq 0 -and $env:OSF_RELEASE_SELFTEST_MODE -ne 'lying-pass'
@{kind='assertion'; data=@{passed=$passed}} | ConvertTo-Json -Compress | Set-Content -LiteralPath (Join-Path $path 'events.jsonl')
$outcome = if ($code -eq 0) {'passed'} elseif ($code -eq 2) {'blocked'} else {'failed'}
@{exitCode=$code; run=@{startedAt=$started; path=$path; outcome=$outcome}} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ResultPath
exit $code
'@ | Set-Content -LiteralPath (Join-Path $fake 'Test-Starfield.ps1')
$oldMode = $env:OSF_RELEASE_SELFTEST_MODE
try {
    foreach ($mode in @('passed','failed','blocked','no-receipt','lying-pass','build-fail','filtered')) {
        $env:OSF_RELEASE_SELFTEST_MODE = $mode
        $receipt = Join-Path $scratch "$mode-result.json"
        $arguments = @('-NoProfile','-File',(Join-Path $fake 'Test-SettingsRelease.ps1'),'-ResultPath',$receipt)
        if ($mode -eq 'filtered') { $arguments += @('-Cases','features-normal') }
        & pwsh @arguments > (Join-Path $scratch "$mode.log") 2>&1
        $exitCode = $LASTEXITCODE
        $result = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json -AsHashtable
        if ($mode -eq 'passed') {
            Check ($exitCode -eq 0 -and $result.completeSuite -and $result.automatedPassed -and $result.runs.Count -eq 15) 'Full suite can pass with 15 fresh receipts'
        } elseif ($mode -eq 'filtered') {
            Check ($exitCode -eq 0 -and -not $result.completeSuite -and $result.automatedPassed) 'Filtered pass never becomes complete suite'
        } elseif ($mode -eq 'failed') {
            Check ($exitCode -eq 1 -and -not $result.automatedPassed -and $result.runs[-1].outcome -eq 'failed') 'Assertion failures retained while later cases execute'
        } else {
            Check ($exitCode -eq 2 -and -not $result.automatedPassed -and -not $result.completeSuite) "$mode cannot become a pass"
        }
        Check (-not $result.releaseAcceptanceComplete -and -not $result.physicalControllerVerified -and -not $result.productionArchiveRuntimeVerified) 'Automation never invents manual or production-runtime acceptance'
    }
    # The successful filtered run left a stamp. Corrupt a staged file and prove
    # -SkipBuild refuses it before any case starts.
    Add-Content -LiteralPath (Join-Path $fake 'staging/OSF Settings/SFSE/Plugins/OSFSettings.dll') -Value 'changed'
    $receipt = Join-Path $scratch 'stale-result.json'
    & pwsh -NoProfile -File (Join-Path $fake 'Test-SettingsRelease.ps1') -SkipBuild -ResultPath $receipt > (Join-Path $scratch 'stale.log') 2>&1
    $result = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json -AsHashtable
    Check ($LASTEXITCODE -eq 2 -and @($result.runs | Where-Object outcome -ne 'not-run').Count -eq 0) 'Stale staged build blocks before starting cases'
    [IO.File]::WriteAllText((Join-Path $fake 'staging/OSF Settings/SFSE/Plugins/OSFSettings.dll'), 'fake binary')
    [IO.File]::WriteAllText((Join-Path $fake 'overwrite/SFSE/Plugins/OSFSettings.dll'), 'stale override')
    $receipt = Join-Path $scratch 'override-result.json'
    & pwsh -NoProfile -File (Join-Path $fake 'Test-SettingsRelease.ps1') -SkipBuild -ResultPath $receipt > (Join-Path $scratch 'override.log') 2>&1
    $result = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json -AsHashtable
    Check ($LASTEXITCODE -eq 2 -and $result.error -like '*MO2 overrides*' -and @($result.runs | Where-Object outcome -ne 'not-run').Count -eq 0) 'Winning MO2 override blocks before starting cases'
} finally { $env:OSF_RELEASE_SELFTEST_MODE = $oldMode }
Write-Host "$script:checks release validation checks passed. Evidence: $scratch"
