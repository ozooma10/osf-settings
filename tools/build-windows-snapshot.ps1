#requires -Version 7.2
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SnapshotRoot,
    [string]$DependencySource = 'C:\Modding\Starfield\OSF Settings Slim\external'
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

# This entry point only compiles and installs into the snapshot's own staging.
# Extract the source archive into a NEW directory before invoking it.
$snapshot = [IO.Path]::GetFullPath($SnapshotRoot).TrimEnd('\', '/')
$allowed = 'C:\Modding\Starfield\OSF Settings Mac Builds\'
if (-not $snapshot.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Snapshot must be below $allowed"
}
$source = Join-Path $snapshot 'source'
$manifestPath = Join-Path $snapshot 'manifest.json'
$expectedManifest = (Get-Content (Join-Path $snapshot 'manifest.sha256') -Raw).Trim()
if ((Get-FileHash $manifestPath -Algorithm SHA256).Hash -ine $expectedManifest) {
    throw 'Manifest checksum mismatch'
}
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne 1 -or (Split-Path $snapshot -Leaf) -cne $manifest.snapshot_id) {
    throw 'Unexpected snapshot schema or directory name'
}
if ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\', '/') -ine $source) {
    throw 'Run the build script from the transferred source/tools directory'
}
# Refuse junctions/symlinks in the destination ancestry, including the build root.
$ancestor = Get-Item -LiteralPath $source
while ($null -ne $ancestor) {
    if ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point: $($ancestor.FullName)" }
    $ancestor = $ancestor.Parent
}

function Assert-Source([switch]$AfterBuild) {
    $known = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $canonical = [Text.StringBuilder]::new()
    foreach ($entry in $manifest.files) {
        $name = [string]$entry.path
        if ($name -match '[\\:\x00-\x1f]' -or $name.StartsWith('/') -or $name -match '(^|/)(\.|\.\.|\.git)(/|$)' -or -not $known.Add($name)) {
            throw "Unsafe or duplicate manifest path: $name"
        }
        $file = Join-Path $source $name
        $info = Get-Item -LiteralPath $file -Force
        if ($info.PSIsContainer -or ($info.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Not a regular file: $name" }
        if ($info.Length -ne $entry.size -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $entry.sha256) {
            throw "Source checksum mismatch: $name"
        }
        [void]$canonical.Append("$($entry.sha256) $($entry.size) $name`n")
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $identity = [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical.ToString()))).ToLowerInvariant() }
    finally { $sha.Dispose() }
    if ($identity -cne $manifest.source_id) { throw 'Source identity mismatch' }
    # Check unexpected source too; generated directories are permitted only after building.
    foreach ($top in Get-ChildItem -LiteralPath $source -Force) {
        if ($AfterBuild -and ($top.Name -in @('external', 'build', '.xmake', 'compile_commands.json') -or $top.Name -like 'vsxmake*')) { continue }
        $items = @($top)
        if ($top.PSIsContainer) { $items += @(Get-ChildItem -LiteralPath $top.FullName -Force -Recurse) }
        foreach ($item in $items) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point: $($item.FullName)" }
            if (-not $item.PSIsContainer) {
                $relative = [IO.Path]::GetRelativePath($source, $item.FullName).Replace('\', '/')
                if (-not $known.Contains($relative)) { throw "Unexpected source file: $relative" }
            }
        }
    }
}

Assert-Source
foreach ($name in @('reports', 'staging')) {
    if (Test-Path (Join-Path $snapshot $name)) { throw "Existing $name directory; use a fresh snapshot for each build attempt" }
}
$reports = New-Item -ItemType Directory -Path (Join-Path $snapshot 'reports')
$staging = New-Item -ItemType Directory -Path (Join-Path $snapshot 'staging')
$steps = [Collections.Generic.List[object]]::new()
$result = [ordered]@{
    snapshot_id = $manifest.snapshot_id
    source_id = $manifest.source_id
    manifest_sha256 = $expectedManifest
    head = $manifest.repositories[0].head
    project_directory = $source
    staging_directory = $staging.FullName
    host_name = [Environment]::MachineName
    started_utc = [DateTime]::UtcNow.ToString('o')
    status = 'failed'
    exit_code = 1
    source_verified_before = $true
    source_verified_after = $false
    game_launched = $false
    steps = $steps
    artifacts = @()
}

function Invoke-Step([string]$Label, [string]$Executable, [string[]]$Arguments) {
    $log = Join-Path $reports.FullName "$Label.log"
    Write-Host "$Label : $Executable $($Arguments -join ' ')"
    & $Executable @Arguments 2>&1 | Tee-Object -FilePath $log | Out-Host
    $code = $LASTEXITCODE
    $steps.Add([ordered]@{ name = $Label; executable = $Executable; arguments = $Arguments; cwd = (Get-Location).Path; exit_code = $code; log = $log })
    if ($code -ne 0) { throw "$Label failed with exit code $code; see $log" }
}

$oldLocation = Get-Location
$oldMods = $env:XSE_SF_MODS_PATH
$oldGame = $env:XSE_SF_GAME_PATH
try {
    Set-Location -LiteralPath $source
    $env:XSE_SF_MODS_PATH = $staging.FullName
    $env:XSE_SF_GAME_PATH = $null
    $xmake = (Get-Command xmake -CommandType Application -ErrorAction Stop).Source
    $pwsh = (Get-Command pwsh -CommandType Application -ErrorAction Stop).Source
    Invoke-Step 'xmake-version' $xmake @('--version')
    Invoke-Step 'powershell-version' $pwsh @('--version')

    # Copy, rather than link, because setup-scaleform writes into these directories.
    # The original checkout, its configuration, and its dependencies stay untouched.
    if (-not (Test-Path -LiteralPath $DependencySource -PathType Container)) { throw "Missing existing toolchains: $DependencySource" }
    Copy-Item -LiteralPath $DependencySource -Destination (Join-Path $source 'external') -Recurse
    Invoke-Step 'setup-scaleform' $pwsh @('-NoProfile', '-File', 'tools/setup-scaleform.ps1')
    Invoke-Step 'configure' $xmake @('f', '-y', '-m', 'releasedbg', '--test_harness=n')
    Invoke-Step 'compile' $xmake @('build', '-v')
    Invoke-Step 'install' $xmake @('install', 'OSF Settings')
    foreach ($relative in @('SFSE/Plugins/OSFSettings.dll', 'Interface/OSFSettingsMenu.swf', 'Interface/OSFSettingsMenu_LRG.swf')) {
        $payload = Join-Path (Join-Path $staging.FullName 'OSF Settings Slim') $relative
        if (-not (Test-Path -LiteralPath $payload -PathType Leaf)) { throw "Missing staged build output: $payload" }
    }
    $result.artifacts = @(Get-ChildItem -LiteralPath $staging.FullName -File -Recurse | ForEach-Object {
        [ordered]@{ path = [IO.Path]::GetRelativePath($snapshot, $_.FullName); size = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
    $config = Join-Path $source '.xmake/windows/x64/xmake.conf'
    if (Test-Path $config) { Copy-Item -LiteralPath $config -Destination (Join-Path $reports.FullName 'xmake.conf') }
    $result.status = 'passed'
    $result.exit_code = 0
}
catch {
    $result.error = $_.ToString()
    Write-Warning $result.error
}
finally {
    try { Assert-Source -AfterBuild; $result.source_verified_after = $true }
    catch { $result.status = 'failed'; $result.exit_code = 1; $result.source_error = $_.ToString() }
    $env:XSE_SF_MODS_PATH = $oldMods
    $env:XSE_SF_GAME_PATH = $oldGame
    Set-Location -LiteralPath $oldLocation.Path
    $result.finished_utc = [DateTime]::UtcNow.ToString('o')
    $result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $reports.FullName 'result.json') -Encoding utf8
}
Write-Host "Result: $(Join-Path $reports.FullName 'result.json')"
exit $result.exit_code
