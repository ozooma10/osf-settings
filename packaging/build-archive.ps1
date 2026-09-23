#requires -Version 7.2
[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9.-]*$')]
    [string]$Label = 'rc1',
    [string]$ResultPath = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'ReleaseValidation.ps1')
if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'README.txt'))) {
    throw 'Missing packaging input: README.txt'
}
$sourceIdentity = Get-ReleaseSourceIdentity $repo
$project = Get-Content -LiteralPath (Join-Path $repo 'xmake.lua') -Raw
$versionMatch = [regex]::Match($project, 'set_version\("([0-9]+\.[0-9]+\.[0-9]+)"\)')
if (-not $versionMatch.Success) { throw 'Cannot read the project version from xmake.lua.' }
$version = $versionMatch.Groups[1].Value
$name = "OSFSettings-$version-$Label"
$run = Join-Path $repo ("build/packages/{0}-{1}-{2}" -f $name, [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'), [guid]::NewGuid().ToString('N').Substring(0, 8))
$mods = Join-Path $run 'staging'
$stage = Join-Path $mods 'OSF Settings'
New-Item -ItemType Directory -Path $stage -Force | Out-Null

function Invoke-XMake([string[]]$Arguments) {
    & xmake @Arguments
    if ($LASTEXITCODE -ne 0) { throw "xmake $($Arguments -join ' ') failed ($LASTEXITCODE)." }
}

# Route automatic and explicit installs to this run's new staging directory.
# Never package a live MO2 folder, which may contain user values or old fixtures.
$previousMods = $env:XSE_SF_MODS_PATH
$previousGame = $env:XSE_SF_GAME_PATH
Push-Location $repo
try {
    $env:XSE_SF_MODS_PATH = $mods
    $env:XSE_SF_GAME_PATH = $null
    Invoke-XMake @('f', "--project=$repo", '-y', '-p', 'windows', '-a', 'x64', '-m', 'releasedbg', '-o', 'build', '--test_harness=n')
    Invoke-XMake @('build', "--project=$repo", '-y', '-j4', 'OSF Settings')
    Invoke-XMake @('install', "--project=$repo", '-y', 'OSF Settings')
    $revision = & git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record source revision.' }
    $sourceStatus = @(& git status --porcelain=v1 --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record source status.' }
    $submodules = @(& git submodule status --recursive)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record dependency revisions.' }
} finally {
    $env:XSE_SF_MODS_PATH = $previousMods
    $env:XSE_SF_GAME_PATH = $previousGame
    Pop-Location
}

$documents = Join-Path $stage 'Docs/OSFSettings'
New-Item -ItemType Directory -Path $documents -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.txt') -Destination $documents
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE'), (Join-Path $repo 'EXCEPTIONS') -Destination $documents
Copy-Item -LiteralPath (Join-Path $repo 'lib/commonlibsf/COPYING') -Destination (Join-Path $documents 'CommonLibSF-COPYING')
Copy-Item -LiteralPath (Join-Path $repo 'lib/commonlibsf/EXCEPTIONS') -Destination (Join-Path $documents 'CommonLibSF-EXCEPTIONS')
Copy-Item -LiteralPath (Join-Path $repo 'lib/commonlibsf/lib/commonlib-shared/LICENSE') -Destination (Join-Path $documents 'CommonLibShared-LICENSE')
Copy-Item -LiteralPath (Join-Path $repo 'lib/commonlibsf/lib/commonlib-shared/EXCEPTIONS') -Destination (Join-Path $documents 'CommonLibShared-EXCEPTIONS')

$files = @(Get-ReleasePayloadPaths)
$allowedStageFiles = $files + 'SFSE/Plugins/OSFSettings.pdb'
foreach ($file in Get-ChildItem -LiteralPath $stage -File -Recurse) {
    $relative = [IO.Path]::GetRelativePath($stage, $file.FullName).Replace('\', '/')
    if ($relative -cnotin $allowedStageFiles) { throw "Unexpected staged file: $relative" }
}
$manifestFiles = foreach ($relative in $files) {
    $path = Join-Path $stage $relative
    $file = Get-Item -LiteralPath $path
    if ($file.Length -eq 0) { throw "Empty payload: $relative" }
    [ordered]@{ path = $relative; bytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
}

$dllText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Join-Path $stage 'SFSE/Plugins/OSFSettings.dll')))
foreach ($export in @('OSFSettings_TestSnapshot', 'OSFSettings_TestCommand')) {
    if ($dllText.Contains($export)) { throw "Test export present in production DLL: $export" }
}
foreach ($relative in @('Interface/OSFSettingsMenu.swf', 'Interface/OSFSettingsMenu_LRG.swf')) {
    $movie = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Join-Path $stage $relative)))
    if (-not $movie.StartsWith('FWS')) { throw "Expected an uncompressed production movie: $relative" }
    foreach ($marker in @('reportTestState', 'previewConstruct')) {
        if ($movie.Contains($marker)) { throw "Development movie marker $marker in $relative" }
    }
}

$archive = Join-Path $run "$name.zip"
$partial = "$archive.partial"
$zip = [IO.Compression.ZipFile]::Open($partial, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($relative in $files) {
        $entry = $zip.CreateEntry($relative, [IO.Compression.CompressionLevel]::Optimal)
        # Stable ZIP metadata for the same payload; native/PEX builds have their own timestamps.
        $entry.LastWriteTime = [DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
        $inputStream = [IO.File]::OpenRead((Join-Path $stage $relative))
        $outputStream = $entry.Open()
        try { $inputStream.CopyTo($outputStream) }
        finally { $outputStream.Dispose(); $inputStream.Dispose() }
    }
} finally { $zip.Dispose() }

# Reopen the archive and check every entry against the staged payload.
$zip = [IO.Compression.ZipFile]::OpenRead($partial)
try {
    if ($zip.Entries.Count -ne $files.Count) { throw 'Archive entry count mismatch.' }
    for ($i = 0; $i -lt $files.Count; ++$i) {
        $entry = $zip.Entries[$i]
        if ($entry.FullName -cne $files[$i] -or $entry.Length -ne $manifestFiles[$i].bytes) { throw 'Archive entry mismatch.' }
        $stream = $entry.Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = [Convert]::ToHexString($sha.ComputeHash($stream)) }
        finally { $sha.Dispose(); $stream.Dispose() }
        if ($hash -cne $manifestFiles[$i].sha256) { throw "Archive hash mismatch: $($entry.FullName)" }
    }
} finally { $zip.Dispose() }
Move-Item -LiteralPath $partial -Destination $archive
$archiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
"$archiveHash  $name.zip" | Set-Content -LiteralPath "$archive.sha256" -Encoding utf8NoBOM
if ((Get-ReleaseSourceIdentity $repo).sha256 -cne $sourceIdentity.sha256) { throw 'Source changed during packaging; candidate cannot be accepted.' }
$manifestPath = Join-Path $run "$name.manifest.json"
[ordered]@{
    version = $version
    label = $Label
    createdUtc = [DateTime]::UtcNow.ToString('o')
    revision = $revision
    dirty = $sourceStatus.Count -ne 0
    sourceStatus = $sourceStatus
    submodules = $submodules
    sourceIdentity = $sourceIdentity
    configuration = 'windows/x64/releasedbg; test_harness=n'
    archive = "$name.zip"
    sha256 = $archiveHash
    files = @($manifestFiles)
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding utf8NoBOM
$null = Test-ReleaseArchive $manifestPath
if ($ResultPath) { [IO.File]::WriteAllText([IO.Path]::GetFullPath($ResultPath), $manifestPath, [Text.UTF8Encoding]::new($false)) }
Write-Host "Verified archive: $archive"
Write-Host "SHA-256: $archiveHash"
