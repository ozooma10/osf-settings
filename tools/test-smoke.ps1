#requires -Version 7.2
[CmdletBinding()]
param(
    [switch]$RunGame,
    [switch]$Plan,
    [string]$Harness = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'OSF Test Harness'),
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'SmokeBench.ps1')
$coverage = @(Get-Content -LiteralPath (Join-Path $repo 'tests/smoke-coverage.json') -Raw | ConvertFrom-Json)
$suite = Join-Path $Harness 'Test-SettingsRelease.ps1'
$runtimeCases = @()
if (Test-Path -LiteralPath $suite) {
    $runtimePlan = & pwsh -NoProfile -File $suite -Plan | ConvertFrom-Json
    if ($LASTEXITCODE) { throw 'Cannot read the game suite plan.' }
    $runtimeCases = @($runtimePlan.cases)
}
$names = @('coverage', 'native', 'preview-normal', 'preview-large', 'preview-bindings-normal', 'preview-bindings-large',
    'release-contracts', 'production-build', 'runtime', 'source-unchanged')
$unverified = @('Physical controller input and visual review of retained screenshots.',
    'External OSF UI provider handoff and caller-owned provider persistence in a real consumer.',
    'Production ZIP install, upgrade, and restart in game: use tools/test-release.ps1 -RunGame on a committed candidate.')
if (-not $RunGame) { $unverified = @('All in-game cases were skipped; rerun with -RunGame.') + $unverified }
if ($Plan) {
    [ordered]@{ stages=$names; runGame=[bool]$RunGame; runtimeCases=$runtimeCases; coverage=$coverage; unverified=$unverified } | ConvertTo-Json -Depth 10
    exit 0
}
$directory = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else {
    Join-Path $repo ('build/smoke/' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
}
if (Test-Path -LiteralPath $directory) { throw 'Use a new output directory; previous evidence is never overwritten.' }
[IO.Directory]::CreateDirectory($directory) | Out-Null
$report = [ordered]@{
    startedUtc=[DateTime]::UtcNow.ToString('o'); finishedUtc=$null; outcome='running'
    scope=$(if ($RunGame) { 'offline+runtime' } else { 'offline' }); completeAutomatedSuite=$false; automatedPassed=$false
    source=$null; runtime=$null; coverage=$coverage; unverified=$unverified
    stages=@($names | ForEach-Object { [ordered]@{ name=$_; status='not-run'; seconds=0; message=''; log=$null } })
}
$exitCode = 2
$lock = $null
function Invoke-SmokeStage([string]$Name, [scriptblock]$Body) {
    $script:stage = @($report.stages | Where-Object name -eq $Name)[0]
    $script:stage.status = 'running'
    $watch = [Diagnostics.Stopwatch]::StartNew()
    Write-Host "Smoke bench: $Name"
    Save-SmokeReport $report $directory
    try { & $Body; $script:stage.status = 'passed' }
    catch {
        $script:stage.status = if ($_.Exception.Message.StartsWith('[failed]')) { 'failed' } else { 'blocked' }
        $script:stage.message = $_.Exception.Message
        Write-Warning "$Name`: $($_.Exception.Message)"
    } finally {
        $script:stage.seconds = [Math]::Round($watch.Elapsed.TotalSeconds, 3)
        Save-SmokeReport $report $directory
    }
}
function Invoke-SmokeCommand([string]$Program, [string[]]$Arguments) {
    $script:stage.log = Join-Path $directory ($script:stage.name + '.log')
    & $Program @Arguments 2>&1 | Tee-Object -FilePath $script:stage.log -Append | Out-Host
    if ($LASTEXITCODE) {
        $kind = if ($Program -eq 'xmake' -or $LASTEXITCODE -eq 1) { 'failed' } else { 'blocked' }
        throw "[$kind] $Program returned $LASTEXITCODE; see $($script:stage.log)"
    }
}
Push-Location $repo
try {
    # Previews and XMake share output/configuration. Serialize bench invocations.
    [IO.Directory]::CreateDirectory((Join-Path $repo 'build')) | Out-Null
    $lock = [IO.File]::Open((Join-Path $repo 'build/smoke.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
    $identity = Get-SmokeSourceIdentity $repo
    $identity | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $directory 'source.json')
    $report.source = [ordered]@{ revision=$identity.revision; sha256=$identity.sha256; dirty=$identity.dirty }
    Invoke-SmokeStage 'coverage' {
        $defined = @([regex]::Matches((Get-Content (Join-Path $repo 'tests/xmake.lua') -Raw), '"osfsettings-([a-z-]+)-tests"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        $mapped = @($coverage.native | Sort-Object -Unique)
        if (@(Compare-Object $defined $mapped).Count) { throw '[failed] Native suite inventory and smoke-coverage.json disagree.' }
        if ($RunGame -and -not $runtimeCases.Count) { throw '[blocked] Game suite is missing; pass -Harness.' }
        if ($runtimeCases.Count -and @($coverage.runtime | Where-Object { $_ -notin $runtimeCases }).Count) { throw '[failed] Coverage references unknown game cases; update the sibling harness.' }
        $script:stage.message = "$($defined.Count) native suites; $($runtimeCases.Count) game cases; $($coverage.Count) feature areas"
    }
    Invoke-SmokeStage 'native' {
        Invoke-SmokeCommand 'xmake' @('test','-j4','-v')
        $log = Get-Content -LiteralPath $script:stage.log -Raw
        Test-SmokeNativeSummary $log @($coverage.native | Sort-Object -Unique).Count
    }
    foreach ($variant in @(@{name='normal';args=@()}, @{name='large';args=@('-LargeText')},
        @{name='bindings-normal';args=@('-Bindings')}, @{name='bindings-large';args=@('-Bindings','-LargeText')})) {
        Invoke-SmokeStage ('preview-' + $variant.name) {
            $started = [DateTime]::UtcNow
            Invoke-SmokeCommand 'pwsh' (@('-NoProfile','-File',(Join-Path $PSScriptRoot 'test-menu-preview.ps1'),'-Capture') + $variant.args)
            $prefix = if ($variant.name -like '*large') { 'large' } else { 'normal' }
            $captures = @(Get-ChildItem -LiteralPath (Join-Path $repo 'build/preview') -Filter "$prefix-*.png" -File | Where-Object LastWriteTimeUtc -ge $started)
            if (-not $captures.Count) { throw '[blocked] Preview returned without fresh captures.' }
            $destination = Join-Path $directory $script:stage.name
            [IO.Directory]::CreateDirectory($destination) | Out-Null
            $captures | Copy-Item -Destination $destination
            Copy-Item -LiteralPath (Join-Path $repo "build/preview/test-$prefix-errors.log") -Destination $destination
            $script:stage.message = "$($captures.Count) fresh screenshots"
        }
    }
    Invoke-SmokeStage 'release-contracts' {
        Invoke-SmokeCommand 'pwsh' @('-NoProfile','-File',(Join-Path $repo 'tests/smoke_bench_tests.ps1'))
        Invoke-SmokeCommand 'pwsh' @('-NoProfile','-File',(Join-Path $repo 'tests/release_validation_tests.ps1'),'-Harness',$Harness)
    }
    Invoke-SmokeStage 'production-build' {
        $oldMods = $env:XSE_SF_MODS_PATH; $oldGame = $env:XSE_SF_GAME_PATH
        try {
            $env:XSE_SF_MODS_PATH = Join-Path $directory 'staging'; $env:XSE_SF_GAME_PATH = $null
            Invoke-SmokeCommand 'xmake' @('f','-y','-m','releasedbg','--test_harness=n')
            Invoke-SmokeCommand 'xmake' @('build','-j4','OSF Settings')
            $install = Join-Path $directory 'staging/OSF Settings'
            Invoke-SmokeCommand 'xmake' @('install','-o',$install,'OSF Settings')
            $hashes = @(Get-ChildItem -LiteralPath $install -File -Recurse | Get-FileHash)
            $hashes | Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $directory 'production-hashes.json')
            $script:stage.message = 'Production DLL, Papyrus and both movies built and installed to private staging'
        } finally { $env:XSE_SF_MODS_PATH=$oldMods; $env:XSE_SF_GAME_PATH=$oldGame }
    }
    if ($RunGame) {
        if (@($report.stages | Where-Object status -in @('failed','blocked')).Count) {
            $report.stages[8].message = 'Offline prerequisites failed; game was not launched.'
        } else {
            Invoke-SmokeStage 'runtime' {
                $report.runtime = Join-Path $directory 'runtime-result.json'
                Invoke-SmokeCommand 'pwsh' @('-NoProfile','-File',$suite,'-WorkingTree','-ExpectedSettingsProject',$repo,'-ResultPath',$report.runtime)
                $receipt = Get-Content -LiteralPath $report.runtime -Raw | ConvertFrom-Json
                Test-SmokeRuntimeReceipt $receipt $runtimeCases $identity.sha256
                $script:stage.message = "$($receipt.runs.Count) fresh game cases passed; $($report.runtime)"
            }
        }
    } else { $report.stages[8].message = 'Use -RunGame for automated real-input game scenarios.' }
    Invoke-SmokeStage 'source-unchanged' {
        if ((Get-SmokeSourceIdentity $repo).sha256 -cne $identity.sha256) { throw '[blocked] Source changed during this run; rerun against fixed content.' }
    }
    $exitCode = if (@($report.stages | Where-Object status -eq 'blocked').Count) { 2 }
        elseif (@($report.stages | Where-Object status -eq 'failed').Count) { 1 } else { 0 }
    $report.automatedPassed = $exitCode -eq 0
    $report.completeAutomatedSuite = $report.automatedPassed -and [bool]$RunGame
    $report.outcome = if ($exitCode -eq 2) { 'blocked' } elseif ($exitCode -eq 1) { 'failed' }
        elseif ($RunGame) { 'automated-passed-acceptance-gaps-open' } else { 'offline-passed' }
} catch {
    $report.outcome = 'blocked'; $report.unverified += $_.Exception.Message
    Write-Warning $_.Exception.Message
} finally {
    $report.finishedUtc = [DateTime]::UtcNow.ToString('o')
    Save-SmokeReport $report $directory
    if ($lock) { $lock.Dispose() }
    Pop-Location
    Write-Host "Smoke report: $(Join-Path $directory 'report.md')"
}
exit $exitCode
