#requires -Version 7.2
[CmdletBinding()]
param(
    [switch]$Deploy,
    [string]$ModsPath = $env:XSE_SF_MODS_PATH,
    [string]$Compiler = 'C:\Program Files (x86)\Steam\steamapps\common\Starfield\Tools\Papyrus Compiler\PapyrusCompiler.exe',
    [string]$Imports = 'C:\Modding\Starfield\PapyrusSource'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$stage = Join-Path $repo 'build/acceptance-mod/Data'
$flags = Join-Path $Imports 'Starfield_Papyrus_Flags.flg'
if (!(Test-Path -LiteralPath $Compiler) -or !(Test-Path -LiteralPath $flags)) { throw 'Papyrus compiler/imports missing.' }
if ($Deploy -and (Get-Process Starfield -ErrorAction SilentlyContinue)) { throw 'Close Starfield before deploying the test plugin.' }
if ($Deploy -and (!$ModsPath -or !(Test-Path -LiteralPath $ModsPath -PathType Container))) { throw 'Supply the existing MO2 mods directory with -ModsPath.' }

Push-Location $repo
try {
    & xmake f -y -m releasedbg --test_harness=n
    if ($LASTEXITCODE) { throw 'XMake configuration failed.' }
    & xmake build -y -j4 osfsettings-acceptance-mod
    if ($LASTEXITCODE) { throw 'Test plugin build failed.' }
    & xmake build -y -j4 osfsettings-acceptance-assets
    if ($LASTEXITCODE) { throw 'Asset validator build failed.' }
    & xmake run osfsettings-acceptance-assets
    if ($LASTEXITCODE) { throw 'Production schema parser rejected the test fixture.' }
    $scripts = Join-Path $stage 'Scripts'
    [IO.Directory]::CreateDirectory($scripts) | Out-Null
    & $Compiler (Join-Path $PSScriptRoot 'papyrus') "-i=$PSScriptRoot/papyrus;$repo/data/Scripts/Source;$Imports" "-o=$scripts" "-f=$flags" -all
    if ($LASTEXITCODE) { throw 'Papyrus test script compilation failed.' }
    & (Join-Path $PSScriptRoot 'build-fixture.ps1') -OutputDirectory $stage
    & node --check (Join-Path $PSScriptRoot 'data/SFSE/Plugins/OSF/UI/views/osfsettings-test/panel/main.js')
    if ($LASTEXITCODE) { throw 'Web panel JavaScript failed syntax validation.' }

    $payload = [ordered]@{}
    foreach ($file in Get-ChildItem (Join-Path $PSScriptRoot 'data') -Recurse -File) {
        $relative = [IO.Path]::GetRelativePath((Join-Path $PSScriptRoot 'data'),$file.FullName).Replace('\','/')
        if ($file.Extension -eq '.json') { $null = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json }
        $payload[$relative] = $file.FullName
    }
    $payload['SFSE/Plugins/OSFSettingsTestMod.dll'] = Join-Path $repo 'build/windows/x64/releasedbg/OSFSettingsTestMod.dll'
    $payload['Docs/OSFSettingsTestMod/README.md'] = Join-Path $PSScriptRoot 'README.md'
    foreach ($file in Get-ChildItem (Join-Path $PSScriptRoot 'papyrus') -Filter '*.psc') {
        $payload["Scripts/Source/$($file.Name)"] = $file.FullName
        $pex = [IO.Path]::ChangeExtension($file.Name,'.pex')
        $payload["Scripts/$pex"] = Join-Path $scripts $pex
    }
    $payload['OSFSettingsTestMod.esm'] = Join-Path $stage 'OSFSettingsTestMod.esm'
    $hashes = @()
    foreach ($entry in $payload.GetEnumerator()) {
        if (!(Test-Path -LiteralPath $entry.Value -PathType Leaf) -or (Get-Item -LiteralPath $entry.Value).Length -eq 0) { throw "Missing/empty payload: $($entry.Key)" }
        $destination = Join-Path $stage $entry.Key
        [IO.Directory]::CreateDirectory((Split-Path $destination -Parent)) | Out-Null
        if ([IO.Path]::GetFullPath($entry.Value) -ne [IO.Path]::GetFullPath($destination)) { Copy-Item -LiteralPath $entry.Value -Destination $destination -Force }
        $hashes += [ordered]@{ path=$entry.Key; sha256=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash }
    }
    if ($Deploy) {
        $destinationRoot = Join-Path ([IO.Path]::GetFullPath($ModsPath)) 'OSF Settings Test Mod'
        [IO.Directory]::CreateDirectory($destinationRoot) | Out-Null
        foreach ($file in $hashes) {
            $destination = Join-Path $destinationRoot $file.path
            [IO.Directory]::CreateDirectory((Split-Path $destination -Parent)) | Out-Null
            Copy-Item -LiteralPath (Join-Path $stage $file.path) -Destination $destination -Force
            if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -cne $file.sha256) { throw "Deployment mismatch: $($file.path)" }
        }
        Write-Host "Deployed $($hashes.Count) verified files to $destinationRoot"
    }
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path (Split-Path $stage -Parent) 'payload-sha256.json') -Encoding utf8NoBOM
    Write-Host "Staged test mod: $stage"
} finally { Pop-Location }
