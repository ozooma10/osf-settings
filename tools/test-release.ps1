#requires -Version 7.2
[CmdletBinding()]
param(
    [switch]$RunGame,
    [switch]$Plan,
    [string]$Harness = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'OSF Test Harness'),
    [string]$Manifest = '',
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9.-]*$')][string]$Label = 'final-smoke',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $repo 'packaging/ReleaseValidation.ps1')
$stages = @('native', 'preview-normal', 'preview-large', 'package', 'package-integrity', 'reinstall-preservation', 'runtime', 'source-unchanged')
if ($Plan) {
    [ordered]@{
        stages = $stages
        runGame = [bool]$RunGame
        runtimeRunner = (Join-Path $Harness 'Test-SettingsRelease.ps1')
        runtimeCases = @('smoke-1','smoke-2','smoke-3','acceptance-normal','acceptance-large','features-normal','features-large','new-game','cancelled-load','keybindings-normal','keybindings-large','search-input','search-large','translation-registration','failed-load')
        remainingGates = @('Physical controller input', 'Visual screenshot review', 'Clean mod-manager installation and real-consumer gameplay with the unmodified production ZIP', 'External OSF UI provider handoff')
    } | ConvertTo-Json -Depth 4
    exit 0
}
$directory = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else {
    Join-Path $repo ('build/release-validation/' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
}
if (Test-Path -LiteralPath $directory) { throw 'Use a new output directory; previous evidence is never overwritten.' }
[IO.Directory]::CreateDirectory($directory) | Out-Null
$report = [ordered]@{
    startedUtc = [DateTime]::UtcNow.ToString('o'); finishedUtc = $null; outcome = 'running'
    source = $null; package = $null; automatedPassed = $false; completeAutomatedSuite = $false; releaseAcceptanceComplete = $false
    stages = @($stages | ForEach-Object { [ordered]@{ name=$_; status='not-run'; message=''; log=$null } })
    runtime = $null
    unverified = @('Physical controller input', 'Visual review of retained screenshots',
        'Unmodified production ZIP: clean mod-manager installation, real consumer, restart and upgrade in-game',
        'External OSF UI provider handoff; fixture callback coverage is separate')
}
$exitCode = 2
$active = $null

function Save-ReleaseReport {
    $path = Join-Path $directory 'result.json'
    [IO.File]::WriteAllText($path, ($report | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
    $lines = @('# OSF Settings final release validation', '', "Outcome: **$($report.outcome)**", '',
        "Automated checks passed: $($report.automatedPassed)", "Complete automated suite: $($report.completeAutomatedSuite)",
        "Release acceptance complete: $($report.releaseAcceptanceComplete)", '', '| Stage | Result | Evidence |', '| --- | --- | --- |')
    foreach ($stage in $report.stages) { $lines += "| $($stage.name) | $($stage.status) | $($stage.message.Replace('|','/')) |" }
    if ($report.package) { $lines += @('', "Archive: $($report.package.archive)", "SHA-256: $($report.package.sha256)") }
    if ($report.runtime) { $lines += @('', "Game suite: $($report.runtime)") }
    $lines += @('', '## Still requires acceptance', '')
    foreach ($gate in $report.unverified) { $lines += "- $gate" }
    $lines += @('', 'Package integrity and disposable reinstall checks do not establish production gameplay. Runtime scenarios use instrumented builds from the same recorded source.')
    [IO.File]::WriteAllLines((Join-Path $directory 'report.md'), $lines, [Text.UTF8Encoding]::new($false))
}

function Start-ReleaseStage([string]$Name) {
    $script:active = @($report.stages | Where-Object name -eq $Name)[0]
    $script:active.status = 'running'
    Write-Host "Release validation: $Name"
    Save-ReleaseReport
}
function Complete-ReleaseStage([string]$Message = '') {
    $script:active.status = 'passed'; $script:active.message = $Message
    Save-ReleaseReport
}
function Invoke-ReleaseProcess([string]$Program, [string[]]$Arguments) {
    $log = Join-Path $directory ($script:active.name + '.log')
    $script:active.log = $log
    & $Program @Arguments 2>&1 | Tee-Object -FilePath $log | Out-Host
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        $script:exitCode = if ($code -eq 1) { 1 } else { 2 }
        throw "$Program returned $code; see $log"
    }
}

Push-Location $repo
try {
    $report.source = Get-ReleaseSourceIdentity $repo
    Start-ReleaseStage 'native'
    Invoke-ReleaseProcess 'xmake' @('test','-j4')
    Complete-ReleaseStage 'All native suites'
    foreach ($large in @($false,$true)) {
        Start-ReleaseStage $(if ($large) { 'preview-large' } else { 'preview-normal' })
        $arguments = @('-NoProfile','-File',(Join-Path $PSScriptRoot 'test-menu-preview.ps1'),'-Capture')
        if ($large) { $arguments += '-LargeText' }
        Invoke-ReleaseProcess 'pwsh' $arguments
        $prefix = if ($large) { 'large' } else { 'normal' }
        $captures = @(Get-ChildItem -LiteralPath (Join-Path $repo 'build/preview') -Filter "$prefix-*.png" -File | Where-Object LastWriteTimeUtc -ge ([DateTime]::Parse($report.startedUtc)))
        if (-not $captures.Count) { throw 'Preview passed without fresh screenshots.' }
        $captureDirectory = Join-Path $directory $script:active.name
        [IO.Directory]::CreateDirectory($captureDirectory) | Out-Null
        $captures | Copy-Item -Destination $captureDirectory
        Complete-ReleaseStage "$($captures.Count) fresh screenshots retained"
    }
    Start-ReleaseStage 'package'
    if ($Manifest) { $Manifest = [IO.Path]::GetFullPath($Manifest); Complete-ReleaseStage 'Explicit existing candidate; source identity is checked next' }
    else {
        $receipt = Join-Path $directory 'package-path.txt'
        Invoke-ReleaseProcess 'pwsh' @('-NoProfile','-File',(Join-Path $repo 'packaging/build-archive.ps1'),'-Label',$Label,'-ResultPath',$receipt)
        $Manifest = (Get-Content -LiteralPath $receipt -Raw).Trim()
        Complete-ReleaseStage $Manifest
    }
    Start-ReleaseStage 'package-integrity'
    $report.package = Test-ReleaseArchive $Manifest $repo
    Copy-Item -LiteralPath $Manifest -Destination (Join-Path $directory 'candidate.manifest.json')
    Complete-ReleaseStage "$($report.package.files) allowlisted files; hashes, APIs and production markers checked"
    Start-ReleaseStage 'reinstall-preservation'
    $sentinels = Test-ReleaseReinstall $report.package.archive (Join-Path $directory 'disposable-install')
    $sentinels | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directory 'preserved-state.json')
    Complete-ReleaseStage 'Settings values, launcher history and unrelated Controls sentinel preserved on disk'
    Start-ReleaseStage 'runtime'
    if ($RunGame) {
        $suite = Join-Path $Harness 'Test-SettingsRelease.ps1'
        $receipt = Join-Path $directory 'runtime-result.json'
        Invoke-ReleaseProcess 'pwsh' @('-NoProfile','-File',$suite,'-ResultPath',$receipt)
        $runtime = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json -AsHashtable
        $report.runtime = $receipt
        if (-not $runtime.automatedPassed -or -not $runtime.completeSuite -or $runtime.sourceIdentity.sha256 -cne $report.source.sha256) { throw 'Runtime suite is incomplete or used different source.' }
        Complete-ReleaseStage 'All fresh-session cases passed; instrumented build evidence retained'
    } else {
        $script:active.status = 'not-run'; $script:active.message = 'Use -RunGame to run the automated game suite'
        $report.unverified += 'Automated game suite was not requested'
    }
    Start-ReleaseStage 'source-unchanged'
    if ((Get-ReleaseSourceIdentity $repo).sha256 -cne $report.source.sha256) { throw 'Source changed during validation; rerun against one fixed candidate.' }
    $null = Test-ReleaseArchive $Manifest
    Complete-ReleaseStage 'Source and candidate archive unchanged throughout validation'
    $report.automatedPassed = $true
    $report.completeAutomatedSuite = [bool]$RunGame
    $report.outcome = if ($RunGame) { 'automated-passed-manual-gates-open' } else { 'offline-passed-runtime-not-run' }
    $exitCode = if ($RunGame) { 0 } else { 3 }
} catch {
    if ($script:active) { $script:active.status = if ($exitCode -eq 1) { 'failed' } else { 'blocked' }; $script:active.message = $_.Exception.Message }
    $report.outcome = if ($exitCode -eq 1) { 'failed' } else { 'blocked' }
    if (Test-Path -LiteralPath (Join-Path $directory 'runtime-result.json')) { $report.runtime = Join-Path $directory 'runtime-result.json' }
    Write-Warning $_.Exception.Message
} finally {
    $report.finishedUtc = [DateTime]::UtcNow.ToString('o')
    Save-ReleaseReport
    Pop-Location
    Write-Host "Release report: $(Join-Path $directory 'report.md')"
}
exit $exitCode
