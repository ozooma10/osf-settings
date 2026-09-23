#Requires -Version 7.0
[CmdletBinding()]
param(
    [switch]$LargeText,
    [switch]$Bindings,
    [switch]$Capture,
    [switch]$Force,
    [string]$InterfaceArchive = 'C:/Program Files (x86)/Steam/steamapps/common/Starfield/Data/Starfield - Interface.ba2'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$output = Join-Path $repo 'build/preview'
$ruffle = Join-Path $repo 'external/ruffle/ruffle.exe'
if (-not (Test-Path -LiteralPath $ruffle)) { throw 'Run pwsh tools/setup-ruffle.ps1 first.' }
& "$PSScriptRoot/preview-menu.ps1" -BuildOnly -Design -Issues -LargeText:$LargeText -InterfaceArchive $InterfaceArchive -Force:$Force
$name = if ($LargeText) { 'large' } else { 'normal' }
$log = Join-Path $output "test-$name.log"
$env:RUST_LOG = 'warn,avm_trace=info'
$player = Start-Process -FilePath $ruffle -WorkingDirectory $output -WindowStyle Hidden -ArgumentList @(
    '-Pverify=true', "-PverifyBindings=$($Bindings.IsPresent.ToString().ToLowerInvariant())",
    "-PverifyCaptures=$($Capture.IsPresent.ToString().ToLowerInvariant())", '--width', '1280', '--height', '720', '--storage', 'memory',
    '--config', 'check-config', '--cache-directory', 'ruffle-cache',
    '--filesystem-access-mode', 'allow', '--dummy-external-interface',
    '--open-url-mode', 'deny', '--tcp-connections', 'deny', 'PreviewHost.swf'
) -RedirectStandardOutput $log -RedirectStandardError "$output/test-$name-errors.log" -PassThru
try {
    $deadline = (Get-Date).AddSeconds(30)
    $content = [Text.StringBuilder]::new()
    $reader = $null
    do {
        Start-Sleep -Milliseconds 50
        if (-not $reader) {
            $stream = [IO.File]::Open($log, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
            $reader = [IO.StreamReader]::new($stream)
        }
        $chunk = $reader.ReadToEnd()
        $null = $content.Append($chunk)
        # Keep only a short rolling suffix for markers split across writes.
        $tail = $content.ToString([Math]::Max(0, $content.Length - 512), [Math]::Min(512, $content.Length))
    } while ((Get-Date) -lt $deadline -and -not $player.HasExited -and $tail -notmatch '\[verify\] (PASS|FAIL)')
    $content = $content.ToString()
    foreach ($match in [regex]::Matches($content, '\[preview-png:([a-z-]+)\] ([A-Za-z0-9+/=]+)')) {
        $image = Join-Path $output "$name-$($match.Groups[1].Value).png"
        [IO.File]::WriteAllBytes($image, [Convert]::FromBase64String($match.Groups[2].Value))
    }
    foreach ($match in [regex]::Matches($content, '\[verify\][^\r\n]*')) { Write-Host $match.Value }
    if ($content -notmatch '\[verify\] PASS') { throw "Preview checks failed or timed out. See $log" }
    if ($Capture) { Write-Host "Rendered captures: $output/$name-*.png" }
} finally {
    if ($reader) { $reader.Dispose() }
    if (-not $player.HasExited) { $player.Kill() }
    $player.Dispose()
}
