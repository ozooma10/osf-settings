#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$InterfaceArchive = 'C:/Program Files (x86)/Steam/steamapps/common/Starfield/Data/Starfield - Interface.ba2',
    [switch]$LargeText,
    [switch]$Scrolling,
    [switch]$Design,
    [switch]$Issues,
    [switch]$BuildOnly,
    [switch]$Watch,
    [ValidateRange(640, 7680)][int]$Width = 1280,
    [ValidateRange(360, 4320)][int]$Height = 720
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$output = Join-Path $repo 'build/preview'
$ruffle = Join-Path $repo 'external/ruffle/ruffle.exe'
$flex = Join-Path $repo 'external/flex'
$compiler = Join-Path $flex 'bin/mxmlc.bat'
$player = Join-Path $flex 'frameworks/libs/player/10.3/playerglobal.swc'
$schemas = @((Join-Path $repo 'data/SFSE/Plugins/OSF/Settings/schemas/learning.json'))
if ($Design) { $schemas = @((Join-Path $repo 'tests/menu/design-preview.json')) }
if ($Scrolling) { $schemas += Join-Path $repo 'tests/menu/scrolling.json' }
if ($Watch -and $BuildOnly) { throw 'Use either -Watch or -BuildOnly.' }
if (-not (Test-Path -LiteralPath $InterfaceArchive)) { throw "Interface archive missing: $InterfaceArchive. Supply -InterfaceArchive with your installation's path." }
if (-not (Get-Command python -ErrorAction SilentlyContinue)) { throw 'Python 3.9+ is required to read the local game archive.' }
if (-not (Test-Path -LiteralPath $compiler)) { throw 'Run pwsh tools/setup-scaleform.ps1 first.' }
if (-not $BuildOnly -and -not (Test-Path -LiteralPath $ruffle)) { & "$PSScriptRoot/setup-ruffle.ps1" }
New-Item -ItemType Directory -Force -Path $output | Out-Null

function Build-Preview {
    & "$PSScriptRoot/build-scaleform.ps1" -Preview
    $suffix = if ($LargeText) { '_LRG' } else { '' }
    $prepareArgs = @('--archive', $InterfaceArchive, '--output', $output, '--menu', "$repo/build/scaleform/OSFSettingsMenu$suffix.raw.swf")
    if ($LargeText) { $prepareArgs += '--large' }
    if ($Issues) { $prepareArgs += @('--issues', (Join-Path $repo 'tests/menu/issues.json')) }
    & python -B "$PSScriptRoot/prepare-menu-preview.py" @prepareArgs @schemas
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare preview assets.' }
    $env:JAVA_HOME = Join-Path $repo 'external/temurin8'
    & $compiler '-load-config=' '-target-player=10.3' '-swf-version=12' "-external-library-path+=$player" '-use-network=false' '-debug=true' "-output=$output/PreviewHost.swf" "$repo/scaleform/preview/PreviewHost.as"
    if ($LASTEXITCODE -ne 0) { throw 'Could not compile the preview host.' }
}

function Start-Preview {
    $env:RUST_LOG = 'warn,avm_trace=info'
    # Ruffle is the interactive tool requested by the user; show its window.
    Start-Process -FilePath $ruffle -WorkingDirectory $output -ArgumentList @(
        '--width', $Width, '--height', $Height, '--storage', 'memory',
        '--config', 'ruffle-config', '--cache-directory', 'ruffle-cache',
        '--filesystem-access-mode', 'allow', '--dummy-external-interface',
        '--open-url-mode', 'deny', '--tcp-connections', 'deny', 'PreviewHost.swf'
    ) -RedirectStandardOutput "$output/ruffle.log" -RedirectStandardError "$output/ruffle-errors.log" -PassThru
}

function Source-Stamp {
    $files = @(Get-ChildItem -LiteralPath "$repo/scaleform" -Recurse -File)
    $files += Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object { $_.Extension -in '.py', '.ps1' }
    $files += Get-Item -LiteralPath $schemas
    if ($Issues) { $files += Get-Item -LiteralPath (Join-Path $repo 'tests/menu/issues.json') }
    ($files | Sort-Object FullName | ForEach-Object { "$($_.FullName):$($_.LastWriteTimeUtc.Ticks):$($_.Length)" }) -join '|'
}

Build-Preview
if ($BuildOnly) { return }
$previewProcess = Start-Preview
Write-Host 'Ruffle preview: E/Enter toggle, arrows navigate, Tab/Escape back, F5 reset.'
Write-Host "Values stay in memory. Log: $output/ruffle.log"
if ($Watch) {
    Write-Host 'Watching menu source and schemas. Saving rebuilds and restarts this preview. Close Ruffle or press Ctrl+C to stop.'
    $stamp = Source-Stamp
    try {
        while (-not $previewProcess.HasExited) {
            Start-Sleep -Milliseconds 500
            $next = Source-Stamp
            if ($next -eq $stamp) { continue }
            Start-Sleep -Milliseconds 300
            $stamp = Source-Stamp
            try { Build-Preview } catch { Write-Warning $_; continue }
            if ($previewProcess.HasExited) { break }
            $previewProcess.Kill(); $previewProcess.WaitForExit()
            $previewProcess = Start-Preview
        }
    } finally {
        if (-not $previewProcess.HasExited) { $previewProcess.Kill(); $previewProcess.WaitForExit() }
    }
}
