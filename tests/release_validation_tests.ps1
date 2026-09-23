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
Check ($candidate.files -eq 15) 'Valid archive has all expected payloads'
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

# Contract between tools/test-release.ps1 -RunGame and the harness suite. The real
# Test-SettingsRelease.ps1 and Common.ps1 run in a disposable tree whose game runner
# only writes evidence; no game, MO2, input or XMake is used.
$suite = Join-Path $Harness 'Test-SettingsRelease.ps1'
if (-not (Test-Path -LiteralPath $suite)) { Write-Warning "Harness suite not found; contract checks skipped: $suite" }
else {
    $fake = Join-Path $scratch 'harness'
    foreach ($directory in @('scripts', 'artifacts', 'settings/packaging')) { [IO.Directory]::CreateDirectory((Join-Path $fake $directory)) | Out-Null }
    Copy-Item -LiteralPath $suite -Destination $fake
    Copy-Item -LiteralPath (Join-Path $Harness 'scripts/Common.ps1') -Destination (Join-Path $fake 'scripts')
    "@{ MO2 = '$fake'; TestProfile = 'OSF Testing'; OutputDirectory = 'artifacts'; SettingsProject = '$(Join-Path $fake 'settings')'; CommandTimeoutSeconds = 15 }" |
        Set-Content -LiteralPath (Join-Path $fake 'config.psd1')
    'fixture-source' | Set-Content -LiteralPath (Join-Path $fake 'settings/identity.txt')
    'function Get-ReleaseSourceIdentity([string]$Repository) { @{ algorithm = ''self-test''; sha256 = (Get-Content -LiteralPath (Join-Path $Repository ''identity.txt'') -Raw).Trim() } }' |
        Set-Content -LiteralPath (Join-Path $fake 'settings/packaging/ReleaseValidation.ps1')
    @'
param([string]$Action, [string]$Scenario, [switch]$SkipBuild, [switch]$Acceptance, [string]$AcceptancePhase, [switch]$LargeText,
    [switch]$Keybindings, [switch]$TextInput, [switch]$TranslationRegistration, [switch]$NativeBindings)
$mode = $env:OSF_RELEASE_SELFTEST_MODE
if ($Action -eq 'Build') { if ($mode -eq 'build-fail') { exit 2 } else { exit 0 } }
$path = Join-Path $PSScriptRoot ('artifacts/' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff') + '-SettingsSmoke')
[IO.Directory]::CreateDirectory($path) | Out-Null
'# fake run' | Set-Content -LiteralPath (Join-Path $path 'report.md')
if ($mode -eq 'source-changed') { 'edited-source' | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'settings/identity.txt') }
if ($mode -eq 'failed') { exit 1 } elseif ($mode -eq 'blocked') { exit 2 } else { exit 0 }
'@ | Set-Content -LiteralPath (Join-Path $fake 'Test-Starfield.ps1')
    $advertised = (& pwsh -NoProfile -File (Join-Path $repo 'tools/test-release.ps1') -Plan -Harness $fake | ConvertFrom-Json).runtimeCases
    $cases = (& pwsh -NoProfile -File (Join-Path $fake 'Test-SettingsRelease.ps1') -Plan | ConvertFrom-Json).cases
    Check ($cases.Count -eq 9 -and ($advertised -join ',') -ceq ($cases -join ',')) 'Release runner advertises exactly the harness suite cases'
    $oldMode = $env:OSF_RELEASE_SELFTEST_MODE
    try {
        foreach ($mode in @('passed', 'filtered', 'failed', 'blocked', 'build-fail', 'source-changed')) {
            $env:OSF_RELEASE_SELFTEST_MODE = $mode
            $receipt = Join-Path $scratch "$mode-result.json"
            $arguments = @('-NoProfile', '-File', (Join-Path $fake 'Test-SettingsRelease.ps1'), '-ResultPath', $receipt)
            if ($mode -in @('filtered', 'failed')) { $arguments += @('-Cases', 'acceptance-normal,acceptance-large') }
            & pwsh @arguments > (Join-Path $scratch "$mode.log") 2>&1
            $exitCode = $LASTEXITCODE
            $result = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json -AsHashtable
            Check ($result.sourceIdentity.sha256 -ceq 'fixture-source' -and -not $result.physicalControllerVerified) "$mode receipt records built source and no controller acceptance"
            switch ($mode) {
                'passed' { Check ($exitCode -eq 0 -and $result.completeSuite -and $result.automatedPassed -and $result.runs.Count -eq 9 -and @($result.runs | Where-Object outcome -ne 'passed').Count -eq 0) 'Full suite passes with nine fresh receipts' }
                'filtered' { Check ($exitCode -eq 0 -and -not $result.completeSuite -and $result.automatedPassed -and $result.runs.Count -eq 2) 'Filtered pass never becomes a complete suite' }
                'failed' { Check ($exitCode -eq 1 -and -not $result.automatedPassed -and $result.runs.Count -eq 2 -and $result.runs[0].outcome -eq 'failed') 'Assertion failures are retained while later cases execute' }
                'blocked' { Check ($exitCode -eq 2 -and -not $result.automatedPassed -and $result.runs.Count -eq 1 -and $result.error -like '*blocked*') 'A blocked case stops the suite without a pass' }
                'build-fail' { Check ($exitCode -eq 2 -and -not $result.automatedPassed -and $result.runs.Count -eq 0 -and $result.error -like '*build*') 'Build failure blocks before any case' }
                'source-changed' { Check ($exitCode -eq 2 -and -not $result.automatedPassed -and $result.error -like '*source changed*') 'Source edited during the suite cannot pass' }
            }
            if ($mode -eq 'source-changed') { 'fixture-source' | Set-Content -LiteralPath (Join-Path $fake 'settings/identity.txt') }
        }
    } finally { $env:OSF_RELEASE_SELFTEST_MODE = $oldMode }
}

Write-Host "$script:checks release validation checks passed. Evidence: $scratch"
