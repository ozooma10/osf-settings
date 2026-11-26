#requires -Version 7.2
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Label,
    [string]$Fixture = '',
    [string]$Harness = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'OSF Test Harness'),
    [ValidateRange(1,20)][int]$Cycles = 8
)
$ErrorActionPreference = 'Stop'
# Use the existing local runner's ownership, mailbox, deadlines, isolation and cleanup.
# No input or foreground operations. Build/stage SettingsSmoke before invoking this tool.
$Scenario = 'SettingsSmoke'; $Interactive = $false; $TranslationRegistration = $false
$Acceptance = $true; $AcceptancePhase = 'All'; $LargeText = $false
. (Join-Path $Harness 'scripts/Common.ps1')
. (Join-Path $Harness 'scripts/Environment.ps1')
. (Join-Path $Harness 'scripts/Reports.ps1')
. (Join-Path $Harness 'scenarios/SettingsAcceptance.ps1')
$lock = [IO.File]::Open((Join-Path $Harness 'state/runner.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
$outcome = 'blocked'; $message = 'Setup did not complete.'
try {
    if (@(Get-GameProcess).Count) { throw '[blocked] A game is already running.' }
    $null = Start-RunArtifacts ('SettingsPerformance-' + $Label)
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $script:Run.path 'profile-runtime.ps1')
    Initialize-Harness
    Prepare-SettingsFixtures
    $privateProfile = Join-Path $script:Config.MO2 ('profiles/OSF Perf ' + $script:Run.id)
    New-TemporaryMO2Directory profiles $privateProfile
    Copy-Tree $script:ProfileRoot $privateProfile
    $script:Config.TestProfile = Split-Path $privateProfile -Leaf
    $script:ProfileRoot = $privateProfile
    if ($Fixture) {
        $privateMod = Join-Path $script:ModRoot ('OSF Perf ' + $script:Run.id)
        New-TemporaryMO2Directory mods $privateMod
        $content = Join-Path $privateMod 'SFSE/Plugins/OSF/Settings'
        Copy-Tree (Join-Path $Fixture 'schemas') (Join-Path $content 'schemas')
        Copy-Tree (Join-Path $Fixture 'translations') (Join-Path $content 'translations')
        $list = Join-Path $privateProfile 'modlist.txt'
        [IO.File]::WriteAllLines($list, @('+' + (Split-Path $privateMod -Leaf)) + [IO.File]::ReadAllLines($list))
        Write-JsonFile (Join-Path $script:Run.path 'fixture-content.json') @(Get-ChildItem $content -File -Recurse | Get-FileHash | Select-Object Path,Hash)
    }
    Write-JsonFile (Join-Path $script:Run.path 'fixture-seed.json') (Read-JsonFile (Join-Path $script:FixtureRoot 'fixture.json'))
    $null = Start-TestGame
    Write-JsonFile (Join-Path $script:Run.path 'startup.json') (Get-SettingsState)
    $idle = [Diagnostics.Stopwatch]::StartNew()
    $null = Wait-Observed 'ten seconds of loaded idle progression' { Get-GameState } {
        param($s) $idle.Elapsed.TotalSeconds -ge 10 -and -not $s.snapshotStale -and $s.loaded
    } 20
    Write-JsonFile (Join-Path $script:Run.path 'idle.json') (Get-SettingsState)
    for ($cycle=0; $cycle -lt $Cycles; ++$cycle) {
        $watch = [Diagnostics.Stopwatch]::StartNew()
        Invoke-SettingsProbe 'performanceMenu' @{open=$true}
        $opened = Wait-Observed 'Settings movie initialized and settled' { Get-SettingsState } {
            param($s) $s.menuOpen -and $s.ui.ContainsKey('initialized') -and $s.ui.initialized -and -not $s.ui.refreshing
        } 20
        Add-Evidence 'menu-cycle' 'Observed native menu open and movie initialization' @{cycle=$cycle;observedLatencyMs=$watch.Elapsed.TotalMilliseconds;ui=$opened.ui}
        if ($cycle -eq 0) { $null = Save-Capture 'settings-open' }
        Invoke-SettingsProbe 'performanceMenu' @{open=$false}
        $null = Wait-Observed 'Settings menu removed' { Get-SettingsState } { param($s) -not $s.menuOpen } 15
    }
    # Exercise provider replacement, removal and restored registration on the guarded lane.
    $transition = 0
    foreach ($operation in @('providerRegister','providerRegister','providerUnregister','providerRegister','providerUnregister')) {
        Invoke-SettingsProbe $operation
        Write-JsonFile (Join-Path $script:Run.path ($transition.ToString() + '-' + $operation + '.json')) (Get-SettingsState)
        ++$transition
    }
    $saveName = 'osfperf' + [guid]::NewGuid().ToString('N')
    $savePath = Join-Path $script:SaveRoot ($saveName + '.sfs')
    $null = Invoke-ConsoleCommand ('save ' + $saveName)
    $null = Wait-Observed 'private save exists' { if (Test-Path -LiteralPath $savePath) { (Get-Item -LiteralPath $savePath).Length } } { param($s) $s -gt 0 } 30
    Start-Sleep -Milliseconds 1000
    $before = Get-SettingsState
    $sequence = ($before.acceptance.records | Measure-Object sequence -Maximum).Maximum
    $null = Invoke-ConsoleCommand ('load ' + $saveName)
    $null = Wait-Observed 'fresh engine load event' { Get-SettingsState } {
        param($s) @($s.acceptance.records | Where-Object { $_.source -eq 'engine' -and $_.event -eq 'loaded' -and $_.sequence -gt $sequence }).Count -gt 0
    } 60
    $null = Wait-Observed 'loaded gameplay resumed' { Get-GameState } { param($s) $s.loaded -and -not $s.snapshotStale -and $s.menus.Count -eq 0 } 30
    Invoke-SettingsProbe 'performanceMenu' @{open=$true}
    $null = Wait-Observed 'Settings works after load' { Get-SettingsState } { param($s) $s.menuOpen -and $s.ui.ContainsKey('initialized') -and $s.ui.initialized -and -not $s.ui.refreshing } 20
    $null = Save-Capture 'settings-after-load'
    Invoke-SettingsProbe 'performanceMenu' @{open=$false}
    $null = Wait-Observed 'final menu close' { Get-SettingsState } { param($s) -not $s.menuOpen } 15
    $final = Get-SettingsState
    Assert-Test ($final.performance.dropped -eq 0) 'Profiler aggregation lost no scope records' $final.performance
    Write-JsonFile (Join-Path $script:Run.path 'final.json') $final
    $outcome='passed'; $message='Background startup, idle, repeated menu lifetime, provider transitions and save/load completed. Physical input and interactions were not exercised.'
} catch { $message=$_.Exception.Message; throw }
finally {
    try { Stop-TestGame } finally {
        if ($script:Run) { Finish-RunArtifacts $outcome $message }
        $lock.Dispose()
    }
}
