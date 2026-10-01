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
. (Join-Path $PSScriptRoot 'SmokeBench.ps1')
$stages = @('release-contracts', 'native', 'preview-normal', 'preview-large', 'preview-bindings-normal', 'preview-bindings-large', 'package', 'package-integrity', 'reinstall-preservation', 'runtime', 'source-unchanged')
$suite = Join-Path $Harness 'Test-SettingsRelease.ps1'
$suiteFound = Test-Path -LiteralPath $suite -PathType Leaf
$scope = if ($RunGame) { 'offline+runtime' } else { 'offline' }
$unverified = @('Physical controller input', 'Visual review of retained screenshots',
    'Text-editor focus-loss cancellation and Main Menu/Continue limitations',
    'Unmodified production ZIP: clean mod-manager installation, real consumer, restart and upgrade in-game',
    'External OSF UI provider handoff')
if (-not $RunGame) { $unverified = @("In-game acceptance via $suite (use -RunGame)") + $unverified }
if ($Plan) {
    # The harness owns the case list; ask it rather than advertising a copy here.
    [string[]]$runtimeCases = @(if ($suiteFound) {
        $harnessPlan = & pwsh -NoProfile -File $suite -Plan | ConvertFrom-Json
        if ($LASTEXITCODE -ne 0) { throw "$suite -Plan returned $LASTEXITCODE" }
        $harnessPlan.cases
    })
    [ordered]@{
        stages = $stages
        scope = $scope
        runGame = [bool]$RunGame
        runtimeRunner = $suite
        runtimeRunnerFound = $suiteFound
        runtimeCases = $runtimeCases
        unverified = $unverified
    } | ConvertTo-Json -Depth 4
    exit 0
}
if ($RunGame -and -not $suiteFound) { throw "Harness suite not found: $suite (pass -Harness)." }
$directory = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else {
    Join-Path $repo ('build/release-validation/' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
}
if (Test-Path -LiteralPath $directory) { throw 'Use a new output directory; previous evidence is never overwritten.' }
[IO.Directory]::CreateDirectory($directory) | Out-Null
$report = [ordered]@{
    startedUtc = [DateTime]::UtcNow.ToString('o'); finishedUtc = $null; outcome = 'running'
    scope = $scope; revision = $null; package = $null; automatedPassed = $false; completeAutomatedSuite = $false
    stages = @($stages | ForEach-Object { [ordered]@{ name=$_; status='not-run'; message=''; log=$null } })
    runtime = $null
    unverified = $unverified
}
$exitCode = 2
$active = $null

function Save-ReleaseReport {
    $path = Join-Path $directory 'result.json'
    [IO.File]::WriteAllText($path, ($report | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
    $lines = @('# OSF Settings release validation', '', "Outcome: **$($report.outcome)**", '',
        "Scope: $($report.scope)", "Automated checks passed: $($report.automatedPassed)",
        "Complete automated suite (offline and in-game): $($report.completeAutomatedSuite)", '', '| Stage | Result | Evidence |', '| --- | --- | --- |')
    foreach ($stage in $report.stages) { $lines += "| $($stage.name) | $($stage.status) | $($stage.message.Replace('|','/')) |" }
    if ($report.package) { $lines += @('', "Archive: $($report.package.archive)", "SHA-256: $($report.package.sha256)") }
    if ($report.runtime) { $lines += @('', "Game suite receipt: $($report.runtime)") }
    $lines += @('', '## Still requires acceptance', '')
    foreach ($gate in $report.unverified) { $lines += "- $gate" }
    $lines += @('', 'Package integrity and disposable reinstall checks do not establish production gameplay. The game suite uses instrumented builds of the same recorded source, not the production ZIP.')
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
    & $Program @Arguments 2>&1 | Tee-Object -FilePath $log -Append | Out-Host
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        $script:exitCode = if ($code -eq 1) { 1 } else { 2 }
        throw "$Program returned $code; see $log"
    }
}

Push-Location $repo
try {
    $report.revision = Get-ReleaseRevision $repo
    $source = Get-SmokeSourceIdentity $repo
    Start-ReleaseStage 'release-contracts'
    Invoke-ReleaseProcess 'pwsh' @('-NoProfile','-File',(Join-Path $repo 'tests/smoke_bench_tests.ps1'))
    Invoke-ReleaseProcess 'pwsh' @('-NoProfile','-File',(Join-Path $repo 'tests/release_validation_tests.ps1'),'-Harness',$Harness)
    Complete-ReleaseStage 'Package and runtime evidence contract checks'
    Start-ReleaseStage 'native'
    Invoke-ReleaseProcess 'xmake' @('test','-j4','-v')
    $expectedNative = @([regex]::Matches((Get-Content (Join-Path $repo 'tests/xmake.lua') -Raw), '"osfsettings-([a-z-]+)-tests"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique).Count
    Test-SmokeNativeSummary (Get-Content $script:active.log -Raw) $expectedNative
    Complete-ReleaseStage 'All native suites'
    foreach ($variant in @(@{name='normal';args=@()}, @{name='large';args=@('-LargeText')},
        @{name='bindings-normal';args=@('-Bindings')}, @{name='bindings-large';args=@('-Bindings','-LargeText')})) {
        Start-ReleaseStage ('preview-' + $variant.name)
        $previewStarted = [DateTime]::UtcNow
        $arguments = @('-NoProfile','-File',(Join-Path $PSScriptRoot 'test-menu-preview.ps1'),'-Capture') + $variant.args
        Invoke-ReleaseProcess 'pwsh' $arguments
        $prefix = if ($variant.name -like '*large') { 'large' } else { 'normal' }
        $captures = @(Get-ChildItem -LiteralPath (Join-Path $repo 'build/preview') -Filter "$prefix-*.png" -File | Where-Object LastWriteTimeUtc -ge $previewStarted)
        if (-not $captures.Count) { throw 'Preview passed without fresh screenshots.' }
        $captureDirectory = Join-Path $directory $script:active.name
        [IO.Directory]::CreateDirectory($captureDirectory) | Out-Null
        $captures | Copy-Item -Destination $captureDirectory
        Complete-ReleaseStage "$($captures.Count) fresh screenshots retained"
    }
    Start-ReleaseStage 'package'
    if ($Manifest) { $Manifest = [IO.Path]::GetFullPath($Manifest); Complete-ReleaseStage 'Explicit existing candidate; its commit is checked next' }
    else {
        $receipt = Join-Path $directory 'package-path.txt'
        Invoke-ReleaseProcess 'pwsh' @('-NoProfile','-File',(Join-Path $repo 'packaging/build-archive.ps1'),'-Label',$Label,'-ResultPath',$receipt)
        $Manifest = (Get-Content -LiteralPath $receipt -Raw).Trim()
        Complete-ReleaseStage $Manifest
    }
    Start-ReleaseStage 'package-integrity'
    $report.package = Test-ReleaseArchive $Manifest $repo
    Copy-Item -LiteralPath $Manifest -Destination (Join-Path $directory 'candidate.manifest.json')
    Complete-ReleaseStage "$($report.package.files) allowlisted files; checksum, APIs and production markers checked"
    Start-ReleaseStage 'reinstall-preservation'
    $sentinels = Test-ReleaseReinstall $report.package.archive (Join-Path $directory 'disposable-install')
    $sentinels | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $directory 'preserved-state.json')
    Complete-ReleaseStage 'Archive overlay preserved simulated Documents state and another mod schema; game migration unverified'
    Start-ReleaseStage 'runtime'
    if ($RunGame) {
        # Contract with the harness suite: it writes its JSON summary to -ResultPath
        # with completeSuite, automatedPassed and the revision it built from.
        $receipt = Join-Path $directory 'runtime-result.json'
        $report.runtime = $receipt
        $runtimePlan = & pwsh -NoProfile -File $suite -Plan | ConvertFrom-Json
        if ($LASTEXITCODE) { throw 'Cannot read runtime suite plan.' }
        Invoke-ReleaseProcess 'pwsh' @('-NoProfile','-File',$suite,'-ExpectedSettingsProject',$repo,'-ResultPath',$receipt)
        $runtime = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json -AsHashtable
        Test-SmokeRuntimeReceipt $runtime @($runtimePlan.cases) $source.sha256
        if ($runtime.revision -cne $report.revision) { throw 'Runtime suite tested a different commit than this candidate.' }
        Complete-ReleaseStage "$($runtime.runs.Count) fresh-session cases passed: $($runtime.cases -join ', ')"
    } else {
        $script:active.status = 'not-run'; $script:active.message = 'Use -RunGame to run the harness suite'
        Save-ReleaseReport
    }
    Start-ReleaseStage 'source-unchanged'
    if ((Get-ReleaseRevision $repo) -cne $report.revision) { throw 'Source changed during validation; rerun against one fixed candidate.' }
    $null = Test-ReleaseArchive $Manifest
    Complete-ReleaseStage 'Source and candidate archive unchanged throughout validation'
    $report.automatedPassed = $true
    $report.completeAutomatedSuite = [bool]$RunGame
    $report.outcome = if ($RunGame) { 'automated-passed-manual-gates-open' } else { 'offline-passed' }
    $exitCode = 0
} catch {
    if ($script:active) { $script:active.status = if ($exitCode -eq 1) { 'failed' } else { 'blocked' }; $script:active.message = $_.Exception.Message }
    $report.outcome = if ($exitCode -eq 1) { 'failed' } else { 'blocked' }
    if ($report.runtime -and -not (Test-Path -LiteralPath $report.runtime)) { $report.runtime = $null }
    Write-Warning $_.Exception.Message
} finally {
    $report.finishedUtc = [DateTime]::UtcNow.ToString('o')
    Save-ReleaseReport
    Pop-Location
    Write-Host "Release report: $(Join-Path $directory 'report.md')"
}
exit $exitCode
