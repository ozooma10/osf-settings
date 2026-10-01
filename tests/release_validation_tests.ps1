#requires -Version 7.2
[CmdletBinding()]
param([string]$Harness = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'OSF Test Harness'))
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $repo 'packaging/ReleaseValidation.ps1')
. (Join-Path $repo 'tools/SmokeBench.ps1')
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
    try {
        $paths = @(Get-ReleasePayloadPaths)
        if ($Fault -eq 'traversal') { $paths[-1] = '../escaped.dll' }
        if ($Fault -eq 'duplicate') { $paths[-1] = $paths[0] }
        if ($Fault -eq 'missing-file') { $paths = $paths[0..($paths.Count - 2)] }
        if ($Fault -eq 'extra-file') { $paths += 'personal-settings.json' }
        foreach ($path in $paths) {
            $payload = if ($path.EndsWith('.dll')) { 'MZ OSFSettings_RequestAPI OSFSettings_RequestLauncherAPI OSFSettings_RequestDiagnosticsAPI OSFSettings_RequestProvidersAPI' }
                elseif ($path.EndsWith('.swf')) { 'FWS production fixture' }
                elseif ($path.EndsWith('.json')) { '{}' } else { 'fixture' }
            if ($Fault -eq 'test-dll' -and $path.EndsWith('.dll')) { $payload += ' OSFSettings_TestSnapshot' }
            if ($Fault -eq 'test-movie' -and $path.EndsWith('.swf')) { $payload += ' reportTestState' }
            if ($Fault -eq 'missing-provider-api') { $payload = $payload.Replace('OSFSettings_RequestProvidersAPI', '') }
            if ($Fault -eq 'invalid-json' -and $path.EndsWith('.json')) { $payload = '{' }
            [byte[]]$bytes = if ($path.EndsWith('.pex')) { [byte[]](0xDE,0xC0,0x57,0xFA,0) } else { [Text.Encoding]::UTF8.GetBytes($payload) }
            if ($Fault -eq 'bad-pex' -and $path.EndsWith('.pex')) { $bytes[0] = 0 }
            if ($Fault -eq 'empty-file' -and $path.EndsWith('.dll')) { $bytes = @() }
            $entry = $zip.CreateEntry($path)
            $stream = $entry.Open()
            try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
    $hash = (Get-FileHash -LiteralPath $archive).Hash
    "$hash  candidate.zip" | Set-Content -LiteralPath "$archive.sha256"
    $manifest = Join-Path $directory 'candidate.manifest.json'
    @{ archive='candidate.zip'; sha256=$hash; revision=('a' * 40); configuration='windows/x64/releasedbg; test_harness=n' } | ConvertTo-Json | Set-Content -LiteralPath $manifest
    return $manifest
}
$good = New-Candidate 'valid'
$candidate = Test-ReleaseArchive $good
Check ($candidate.files -eq 15) 'Valid archive has all expected payloads'
$preserved = Test-ReleaseReinstall $candidate.archive (Join-Path $scratch 'reinstall')
Check ($preserved.Count -eq 4) 'Archive overlay preserves simulated Documents state and another mod schema'
foreach ($fault in @(
    @('traversal','Unexpected or duplicate'), @('duplicate','Unexpected or duplicate'),
    @('test-dll','Instrumented DLL'), @('test-movie','Development movie'),
    @('bad-pex','PEX header'), @('missing-file','payload count'), @('extra-file','payload count'),
    @('empty-file','Empty or oversized'), @('missing-provider-api','Missing public API'), @('invalid-json','Conversion from JSON')
)) {
    $manifest = New-Candidate $fault[0] $fault[0]
    Reject { Test-ReleaseArchive $manifest } $fault[1]
}
$originalManifest = Get-Content $good -Raw | ConvertFrom-Json -AsHashtable
foreach ($fault in @(
    @{key='configuration';value='windows/x64/releasedbg; test_harness=y';error='production configuration'},
    @{key='archive';value='../candidate.zip';error='archive filename'},
    @{key='revision';value='';error='commit revision'}
)) {
    $changed = $originalManifest.Clone(); $changed[$fault.key] = $fault.value
    $changed | ConvertTo-Json | Set-Content $good
    Reject { Test-ReleaseArchive $good } $fault.error
}
$originalManifest | ConvertTo-Json | Set-Content $good
'wrong checksum' | Set-Content "$($candidate.archive).sha256"
Reject { Test-ReleaseArchive $good } 'Checksum sidecar'
"$($candidate.sha256)  candidate.zip" | Set-Content "$($candidate.archive).sha256"
Add-Content -LiteralPath $candidate.archive -Value 'tampered'
Reject { Test-ReleaseArchive $good } 'archive checksum'

# A candidate is a clean commit: edits and untracked files block it.
$source = Join-Path $scratch 'source'
[IO.Directory]::CreateDirectory($source) | Out-Null
& git -C $source init -q
'tracked' | Set-Content -LiteralPath (Join-Path $source 'tracked.txt')
& git -C $source add tracked.txt
& git -C $source -c user.name=selftest -c user.email=selftest@invalid commit -qm fixture
Check ((Get-ReleaseRevision $source) -ceq (& git -C $source rev-parse HEAD)) 'A clean checkout is identified by its commit'
$otherCommit = New-Candidate 'different-commit'
Reject { Test-ReleaseArchive $otherCommit $source } 'different commit'
'stray' | Set-Content -LiteralPath (Join-Path $source 'stray.cpp')
Reject { Get-ReleaseRevision $source } 'stray.cpp'
Remove-Item -LiteralPath (Join-Path $source 'stray.cpp')
'edited' | Set-Content -LiteralPath (Join-Path $source 'tracked.txt')
Reject { Get-ReleaseRevision $source } 'tracked.txt'
& git -C $source add tracked.txt
Reject { Get-ReleaseRevision $source } 'tracked.txt'
$null = Invoke-SmokeGit $source @('-c','user.name=selftest','-c','user.email=selftest@invalid','commit','-qm','fixture edit')
$dependency = Join-Path $scratch 'dependency'
[IO.Directory]::CreateDirectory($dependency) | Out-Null
$null = Invoke-SmokeGit $dependency @('init','-q')
'dependency' | Set-Content (Join-Path $dependency 'source.cpp')
$null = Invoke-SmokeGit $dependency @('add','.')
$null = Invoke-SmokeGit $dependency @('-c','user.name=selftest','-c','user.email=selftest@invalid','commit','-qm','dependency')
$null = Invoke-SmokeGit $source @('-c','protocol.file.allow=always','submodule','add','-q',$dependency,'deps/fixture')
$null = Invoke-SmokeGit $source @('-c','user.name=selftest','-c','user.email=selftest@invalid','commit','-qam','dependency pin')
$null = Get-ReleaseRevision $source
$beforeDependency = Get-SmokeSourceIdentity $source
'edited dependency' | Set-Content (Join-Path $source 'deps/fixture/source.cpp')
Reject { Get-ReleaseRevision $source } 'deps/fixture'
Check ($beforeDependency.sha256 -cne (Get-SmokeSourceIdentity $source).sha256) 'Dependency edits change the smoke source fingerprint'
'dependency' | Set-Content (Join-Path $source 'deps/fixture/source.cpp')
'new source' | Set-Content (Join-Path $source 'deps/fixture/untracked.cpp')
Reject { Get-ReleaseRevision $source } 'deps/fixture'
Check ($beforeDependency.sha256 -cne (Get-SmokeSourceIdentity $source).sha256) 'Untracked dependency sources change the smoke fingerprint'

# Run the actual suite and source/evidence helpers with only the game process
# replaced. Private MO2 helper implementation details do not belong to this contract.
$suite = Join-Path $Harness 'Test-SettingsRelease.ps1'
if (-not (Test-Path -LiteralPath $suite)) { throw "Harness contract cannot be verified; pass -Harness: $suite" }
$fake = Join-Path $scratch 'harness'
foreach ($directory in @('scripts', 'artifacts', 'settings/packaging', 'settings/tools')) { [IO.Directory]::CreateDirectory((Join-Path $fake $directory)) | Out-Null }
Copy-Item -LiteralPath $suite -Destination $fake
Copy-Item -LiteralPath (Join-Path $Harness 'scripts/Common.ps1') -Destination (Join-Path $fake 'scripts')
Copy-Item -LiteralPath (Join-Path $repo 'packaging/ReleaseValidation.ps1') -Destination (Join-Path $fake 'settings/packaging')
Copy-Item -LiteralPath (Join-Path $repo 'tools/SmokeBench.ps1') -Destination (Join-Path $fake 'settings/tools')
$settings = Join-Path $fake 'settings'
"@{ MO2 = '$($fake.Replace("'", "''"))'; TestProfile = 'OSF Testing'; OutputDirectory = 'artifacts'; SettingsProject = '$($settings.Replace("'", "''"))'; CommandTimeoutSeconds = 15 }" |
    Set-Content -LiteralPath (Join-Path $fake 'config.psd1')
'fixture' | Set-Content (Join-Path $settings 'identity.txt')
'fixture' | Set-Content (Join-Path $fake 'identity.txt')
@('artifacts/', 'settings/') | Set-Content (Join-Path $fake '.gitignore')
@'
[CmdletBinding()]
param([string]$Action, [string]$Scenario, [switch]$SkipBuild, [switch]$Acceptance, [string]$AcceptancePhase, [switch]$LargeText,
    [switch]$Keybindings, [switch]$TextInput, [switch]$TranslationRegistration, [switch]$NativeBindings, [string]$ResultPath)
$ErrorActionPreference = 'Stop'
$mode = $env:OSF_RELEASE_SELFTEST_MODE
if ($Action -eq 'Build') { if ($mode -eq 'build-fail') { exit 2 } else { exit 0 } }
$started = [DateTimeOffset]::UtcNow
$path = Join-Path $PSScriptRoot ('artifacts/' + [guid]::NewGuid().ToString('N') + '-SettingsSmoke')
[IO.Directory]::CreateDirectory($path) | Out-Null
'# fake run' | Set-Content -LiteralPath (Join-Path $path 'report.md')
$code = switch ($mode) { failed { 1 } blocked { 2 } crashed { 7 } default { 0 } }
$outcome = switch ($code) { 0 { 'passed' } 1 { 'failed' } default { 'blocked' } }
$result = @{ scenario='SettingsSmoke'; path=$path; outcome=$outcome; startedAt=$started.ToString('o'); finishedAt=[DateTimeOffset]::UtcNow.ToString('o') }
if ($AcceptancePhase -eq 'Features') { $result.settingsFeatures = @{ values=$true; actions=$true; launchers=$true; providers=$true; largeText=[bool]$LargeText } }
switch ($mode) {
    'stale-receipt' { $result.startedAt = $started.AddDays(-1).ToString('o') }
    'reversed-times' { $result.finishedAt = $started.AddSeconds(-10).ToString('o') }
    'wrong-scenario' { $result.scenario = 'CharacterStudioSmoke' }
    'contradictory-receipt' { $result.outcome = 'failed' }
}
if ($mode -ne 'missing-receipt') { $result | ConvertTo-Json | Set-Content -LiteralPath $ResultPath }
if ($mode -eq 'malformed-receipt') { '{' | Set-Content -LiteralPath $ResultPath }
if ($mode -in @('source-changed','working-tree-changed')) { 'edited' | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'settings/identity.txt') }
if ($mode -eq 'harness-changed') { 'edited' | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'identity.txt') }
exit $code
'@ | Set-Content -LiteralPath (Join-Path $fake 'Test-Starfield.ps1')
foreach ($repository in @($settings, $fake)) {
    $null = Invoke-SmokeGit $repository @('init','-q')
    $null = Invoke-SmokeGit $repository @('add','.')
    $null = Invoke-SmokeGit $repository @('-c','user.name=selftest','-c','user.email=selftest@invalid','commit','-qm','fixture')
}
$revision = Get-ReleaseRevision $settings
$advertised = (& pwsh -NoProfile -File (Join-Path $repo 'tools/test-release.ps1') -Plan -Harness $fake | ConvertFrom-Json).runtimeCases
if ($LASTEXITCODE) { throw 'Release plan failed.' }
$cases = @((& pwsh -NoProfile -File (Join-Path $fake 'Test-SettingsRelease.ps1') -Plan | ConvertFrom-Json).cases)
if ($LASTEXITCODE) { throw 'Harness plan failed.' }
Check ($cases.Count -gt 0 -and ($advertised -join ',') -ceq ($cases -join ',')) 'Release runner advertises exactly the harness cases'
$coverage = @(Get-Content (Join-Path $repo 'tests/smoke-coverage.json') -Raw | ConvertFrom-Json)
Check (@($coverage.runtime | Where-Object { $_ -notin $cases }).Count -eq 0) 'Every advertised coverage case exists in the harness'
$oldMode = $env:OSF_RELEASE_SELFTEST_MODE
try {
    foreach ($mode in @('passed', 'filtered', 'failed', 'blocked', 'crashed', 'build-fail', 'source-changed', 'harness-changed',
        'missing-receipt', 'stale-receipt', 'reversed-times', 'wrong-scenario', 'contradictory-receipt', 'malformed-receipt',
        'working-tree', 'working-tree-changed', 'dirty-release', 'wrong-project', 'skip-build')) {
        $env:OSF_RELEASE_SELFTEST_MODE = $mode
        'fixture' | Set-Content (Join-Path $settings 'identity.txt')
        'fixture' | Set-Content (Join-Path $fake 'identity.txt')
        if ($mode -like 'working-tree*' -or $mode -eq 'dirty-release') { 'uncommitted' | Set-Content (Join-Path $settings 'identity.txt') }
        $identity = Get-SmokeSourceIdentity $settings
        $receipt = Join-Path $scratch "$mode-result.json"
        $expectedProject = if ($mode -eq 'wrong-project') { $source } else { $settings }
        $arguments = @('-NoProfile', '-File', (Join-Path $fake 'Test-SettingsRelease.ps1'), '-ResultPath', $receipt, '-ExpectedSettingsProject', $expectedProject)
        if ($mode -in @('filtered', 'failed')) { $arguments += @('-Cases', 'acceptance-normal,acceptance-large') }
        if ($mode -like 'working-tree*') { $arguments += '-WorkingTree' }
        if ($mode -eq 'skip-build') { $arguments += '-SkipBuild' }
        & pwsh @arguments > (Join-Path $scratch "$mode.log") 2>&1
        $exitCode = $LASTEXITCODE
        $result = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json -AsHashtable
        Check (-not $result.physicalControllerVerified) "$mode does not claim hardware acceptance"
        switch ($mode) {
            'passed' {
                Check ($exitCode -eq 0 -and $result.revision -ceq $revision) 'Full suite records the built commit'
                Test-SmokeRuntimeReceipt $result $cases $identity.sha256
                Check $true 'Every passing case has a fresh hashed receipt accepted by the caller'
            }
            'filtered' { Check ($exitCode -eq 0 -and -not $result.completeSuite -and $result.automatedPassed -and $result.runs.Count -eq 2) 'Filtered pass never becomes a complete suite' }
            'failed' { Check ($exitCode -eq 1 -and -not $result.automatedPassed -and $result.runs.Count -eq 2 -and $result.runs[0].outcome -eq 'failed') 'Assertion failures remain failures while later cases execute' }
            'working-tree' {
                Check ($exitCode -eq 0 -and $result.source.dirty -and $result.source.sha256 -ceq $identity.sha256) 'Working-tree mode records actual uncommitted content'
                Test-SmokeRuntimeReceipt $result $cases $identity.sha256
            }
            'skip-build' {
                Check ($exitCode -eq 0 -and -not $result.builtFromSource) 'Focused staged rechecks do not claim a source-verified build'
                Reject { Test-SmokeRuntimeReceipt $result $cases $identity.sha256 } 'unverified staged'
            }
            default {
                Check ($exitCode -eq 2 -and -not $result.automatedPassed -and $result.error) "$mode blocks acceptance"
                if ($mode -in @('build-fail','dirty-release','wrong-project')) { Check ($result.runs.Count -eq 0) "$mode blocks before scenarios" }
                elseif ($mode -notin @('source-changed','working-tree-changed','harness-changed')) { Check ($result.runs.Count -eq 1 -and $result.runs[0].outcome -eq 'blocked') "$mode stops at the invalid case" }
            }
        }
    }
} finally { $env:OSF_RELEASE_SELFTEST_MODE = $oldMode }

# Exercise the real single-run entrypoint too: only environment/game/report
# dependencies are replaced, so ResultPath publication and exit handling are real.
$entry = Join-Path $scratch 'entrypoint'
foreach ($directory in @('scripts','scenarios','artifacts')) { [IO.Directory]::CreateDirectory((Join-Path $entry $directory)) | Out-Null }
Copy-Item (Join-Path $Harness 'Test-Starfield.ps1') $entry
Copy-Item (Join-Path $fake 'config.psd1') $entry
Copy-Item (Join-Path $Harness 'scripts/Common.ps1') (Join-Path $entry 'scripts')
@'
function Initialize-Harness {}
function Build-Harness {}
function Prepare-SettingsFixtures {}
function Stop-TestGame {}
function Get-OwnedGame { return $null }
function Start-TestGame { if ($env:OSF_RELEASE_SELFTEST_MODE -eq 'startup-blocked') { throw '[blocked] fixture startup' } }
'@ | Set-Content (Join-Path $entry 'scripts/Environment.ps1')
'' | Set-Content (Join-Path $entry 'scripts/Multiplayer.ps1')
'' | Set-Content (Join-Path $entry 'scripts/Animation.ps1')
@'
function Start-RunArtifacts([string]$Scenario) {
    $script:Run = @{ scenario=$Scenario; startedAt=[DateTimeOffset]::UtcNow.ToString('o'); outcome='running' }
}
function Finish-RunArtifacts([string]$Outcome, [string]$Message) {
    $script:Run.outcome = $Outcome; $script:Run.finishedAt = [DateTimeOffset]::UtcNow.ToString('o')
}
'@ | Set-Content (Join-Path $entry 'scripts/Reports.ps1')
@'
function Invoke-SettingsSmoke { if ($env:OSF_RELEASE_SELFTEST_MODE -eq 'scenario-failed') { throw '[failed] fixture assertion' } }
'@ | Set-Content (Join-Path $entry 'scenarios/Fixture.ps1')
try {
    foreach ($mode in @('passed','scenario-failed','startup-blocked')) {
        $env:OSF_RELEASE_SELFTEST_MODE = $mode
        $receipt = Join-Path $scratch "entry-$mode.json"
        $started = [DateTimeOffset]::UtcNow
        & pwsh -NoProfile -File (Join-Path $entry 'Test-Starfield.ps1') -ResultPath $receipt > (Join-Path $scratch "entry-$mode.log") 2>&1
        $code = $LASTEXITCODE
        $expected = switch ($mode) { passed { 0 } scenario-failed { 1 } startup-blocked { 2 } }
        Check ($code -eq $expected) "Real entrypoint returns the expected $mode exit code"
        $null = Read-SmokeCaseResult $receipt $started ([DateTimeOffset]::UtcNow) $code
        Check $true "Real entrypoint publishes a matching $mode result"
    }
} finally { $env:OSF_RELEASE_SELFTEST_MODE = $oldMode }
Write-Host "$script:checks release validation checks passed. Evidence: $scratch"
