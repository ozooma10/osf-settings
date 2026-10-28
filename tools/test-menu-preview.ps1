#Requires -Version 7.0
[CmdletBinding()]
param(
    [switch]$LargeText,
    [switch]$Bindings,
    [switch]$Capture,
    [switch]$Force,
    [string]$ResultPath,
    [string]$InterfaceArchive = 'C:/Program Files (x86)/Steam/steamapps/common/Starfield/Data/Starfield - Interface.ba2'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'WorkflowCache.ps1')
$output = Join-Path $repo $(if ($LargeText) { 'build/preview/large' } else { 'build/preview/normal' })
$ruffle = Join-Path $repo 'external/ruffle/ruffle.exe'
if (-not (Test-Path -LiteralPath $ruffle)) { throw 'Run pwsh tools/setup.ps1 -Preview first.' }
& "$PSScriptRoot/preview-menu.ps1" -BuildOnly -Design -Issues -LargeText:$LargeText -InterfaceArchive $InterfaceArchive -Force:$Force
$name = if ($LargeText) { 'large' } else { 'normal' }
$case = if ($Bindings) { 'bindings' } else { 'general' }
$captureOutput = Join-Path $output $case
[IO.Directory]::CreateDirectory($captureOutput) | Out-Null
$stamp = Join-Path $output "$case.cache.json"
$receipt = Invoke-WorkflowCached -Stamp $stamp -Force:$Force -Fingerprint {
    [xml]$config = Get-Content -LiteralPath (Join-Path $output 'preview.xml') -Raw
    $inputs = @($PSCommandPath, "$PSScriptRoot/WorkflowCache.ps1", $ruffle,
        "$output/PreviewHost.swf", "$output/menu.swf", "$output/preview.xml")
    $inputs += @($config.preview.libraries.library | ForEach-Object { Join-Path $output $_.url })
    Get-WorkflowFingerprint -Paths $inputs -Values @([string]$LargeText, [string]$Bindings, [string]$Capture)
} -Outputs { @(Get-WorkflowFiles @($captureOutput)) } -Action {
    # Remove only this case's old captures, after checking the exact owned path.
    $owned = [IO.Path]::GetFullPath($captureOutput)
    if (-not $owned.StartsWith([IO.Path]::GetFullPath($repo) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Capture path escaped the repository.' }
    Get-ChildItem -LiteralPath $owned -File | Remove-Item -Force
    $log = Join-Path $captureOutput "test-$name.log"
    $env:RUST_LOG = 'warn,avm_trace=info'
    $player = Start-Process -FilePath $ruffle -WorkingDirectory $output -WindowStyle Hidden -ArgumentList @(
        '-Pverify=true', "-PverifyBindings=$($Bindings.IsPresent.ToString().ToLowerInvariant())",
        "-PverifyCaptures=$($Capture.IsPresent.ToString().ToLowerInvariant())", '--width', '1280', '--height', '720', '--storage', 'memory',
        '--config', ('"' + (Join-Path $captureOutput 'config') + '"'), '--cache-directory', 'ruffle-cache',
        '--filesystem-access-mode', 'allow', '--dummy-external-interface',
        '--open-url-mode', 'deny', '--tcp-connections', 'deny', 'PreviewHost.swf'
    ) -RedirectStandardOutput $log -RedirectStandardError "$captureOutput/test-$name-errors.log" -PassThru
    try {
        # Captures settle each scene for twelve frames and encode PNGs. The expanded
        # regression sequence needs more total time; each stalled step still fails
        # after two seconds inside PreviewChecks.
        $deadline = (Get-Date).AddSeconds($(if ($Capture) { 60 } else { 30 }))
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
            $image = Join-Path $captureOutput "$name-$($match.Groups[1].Value).png"
            [IO.File]::WriteAllBytes($image, [Convert]::FromBase64String($match.Groups[2].Value))
        }
        foreach ($match in [regex]::Matches($content, '\[verify\][^\r\n]*')) { Write-Host $match.Value }
        if ($content -notmatch '\[verify\] PASS') { throw "Preview checks failed or timed out. See $log" }
        if ($Capture) { Write-Host "Rendered captures: $captureOutput/$name-*.png" }
    } finally {
        if ($reader) { $reader.Dispose() }
        if (-not $player.HasExited) { $player.Kill() }
        $player.WaitForExit()
        $player.Dispose()
    }

    if ($Capture -and -not @(Get-ChildItem -LiteralPath $captureOutput -Filter '*.png' -File).Count) { throw 'Preview passed without captures.' }
}
if ($ResultPath) { $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ResultPath -Encoding utf8NoBOM }
