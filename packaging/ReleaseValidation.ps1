# Shared by packaging and the offline release runner. No deployment or game actions.
Set-StrictMode -Version Latest

function Get-ReleaseRevision([string]$Repository) {
    # A candidate is a commit. HEAD pins the submodule commits; status catches edits,
    # untracked sources (src/**.cpp would compile them) and dirty submodules.
    $status = @(& git -C $Repository status --porcelain=v1 --untracked-files=all --ignore-submodules=none)
    if ($LASTEXITCODE) { throw "Cannot read source status: $Repository" }
    if ($status.Count) { throw "Commit or remove local changes before building or validating a release:`n$($status -join "`n")" }
    $revision = & git -C $Repository rev-parse HEAD
    if ($LASTEXITCODE) { throw "Cannot read source revision: $Repository" }
    return $revision
}

function Get-ReleasePayloadPaths {
    return @(
        'Docs/OSFSettings/CommonLibSF-COPYING', 'Docs/OSFSettings/CommonLibSF-EXCEPTIONS',
        'Docs/OSFSettings/CommonLibShared-EXCEPTIONS', 'Docs/OSFSettings/CommonLibShared-LICENSE',
        'Docs/OSFSettings/EXCEPTIONS', 'Docs/OSFSettings/LICENSE', 'Docs/OSFSettings/README.txt',
        'Interface/OSFSettingsMenu.swf',
        'Interface/OSFSettingsMenu_LRG.swf', 'Scripts/OSFSettings.pex', 'Scripts/Source/OSFSettings.psc',
        'SFSE/Plugins/OSF/Settings/schemas/osfsettings.json',
        'SFSE/Plugins/OSF/Settings/translations/en/osfsettings.json',
        'SFSE/Plugins/OSF/Settings/translations/ja/osfsettings.json', 'SFSE/Plugins/OSFSettings.dll'
    )
}

function Test-ReleaseArchive([string]$ManifestPath, [string]$Repository = '') {
    $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json -AsHashtable
    if ($manifest.revision -cnotmatch '^[0-9a-f]{40}$') { throw 'Candidate manifest lacks a valid commit revision.' }
    if ($manifest.configuration -cne 'windows/x64/releasedbg; test_harness=n') { throw 'Candidate is not a production configuration.' }
    if ($manifest.archive -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]*\.zip$') { throw 'Invalid archive filename in manifest.' }
    $archive = Join-Path (Split-Path ([IO.Path]::GetFullPath($ManifestPath)) -Parent) $manifest.archive
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    if ($hash -cne $manifest.sha256) { throw 'Candidate archive checksum mismatch.' }
    $checksum = (Get-Content -LiteralPath "$archive.sha256" -Raw).Trim()
    if ($checksum -cne "$hash  $($manifest.archive)") { throw 'Checksum sidecar mismatch.' }
    if ($Repository -and (Get-ReleaseRevision $Repository) -cne $manifest.revision) {
        throw 'Candidate was built from a different commit; build a new candidate.'
    }
    $allowed = @(Get-ReleasePayloadPaths)
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    try {
        if ($zip.Entries.Count -ne $allowed.Count) { throw 'Archive payload count mismatch.' }
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName
            # Exact allowlist rejects traversal, absolute paths, extra fixtures,
            # case aliases and alternate streams before anything is extracted.
            if ($name -cnotin $allowed -or -not $seen.Add($name)) { throw "Unexpected or duplicate archive entry: $name" }
            if ($entry.Length -le 0 -or $entry.Length -gt 32MB) { throw "Empty or oversized payload: $name" }
            $stream = $entry.Open()
            $buffer = [IO.MemoryStream]::new()
            try { $stream.CopyTo($buffer); $bytes = $buffer.ToArray() } finally { $stream.Dispose(); $buffer.Dispose() }
            if ($name.EndsWith('.dll')) {
                $text = [Text.Encoding]::ASCII.GetString($bytes)
                if ($bytes.Length -lt 2 -or $text.Substring(0, 2) -cne 'MZ') { throw 'Invalid production DLL.' }
                foreach ($symbol in @('OSFSettings_RequestAPI', 'OSFSettings_RequestLauncherAPI', 'OSFSettings_RequestDiagnosticsAPI', 'OSFSettings_RequestProvidersAPI')) {
                    if (-not $text.Contains($symbol)) { throw "Missing public API export marker: $symbol" }
                }
                foreach ($symbol in @('OSFSettings_TestSnapshot', 'OSFSettings_TestCommand')) {
                    if ($text.Contains($symbol)) { throw "Instrumented DLL in archive: $symbol" }
                }
            } elseif ($name.EndsWith('.swf')) {
                $text = [Text.Encoding]::ASCII.GetString($bytes)
                if (-not $text.StartsWith('FWS')) { throw "Expected uncompressed production movie: $name" }
                foreach ($marker in @('reportTestState', 'previewConstruct', 'testSnapshot')) {
                    if ($text.Contains($marker)) { throw "Development movie in archive: $name ($marker)" }
                }
            } elseif ($name.EndsWith('.pex')) {
                if ($bytes.Length -lt 4 -or [Convert]::ToHexString($bytes[0..3]) -ne 'DEC057FA') { throw 'Invalid public Papyrus PEX header.' }
            } elseif ($name.EndsWith('.json')) {
                $null = [Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json -AsHashtable
            }
        }
    } finally { $zip.Dispose() }
    return [ordered]@{ archive = $archive; sha256 = $hash; files = $seen.Count; manifest = [IO.Path]::GetFullPath($ManifestPath) }
}

function Test-ReleaseReinstall([string]$Archive, [string]$Directory) {
    # A disposable Data + Documents tree. This checks archive overwrite behavior;
    # it is deliberately not reported as a game or mod-manager upgrade test.
    if (Test-Path -LiteralPath $Directory) { throw 'Reinstall test requires a new directory.' }
    $data = Join-Path $Directory 'Data'
    [IO.Compression.ZipFile]::ExtractToDirectory($Archive, $data)
    $sentinels = @{
        'Documents/My Games/Starfield/OSF/Settings/osfreleaseprobe.json' = '{"values":{"enabled":true,"caption":"Preserve me"}}'
        'Documents/My Games/Starfield/OSF/Settings/internal.json' = '{"recentLaunchers":[{"mod":"probe","id":"editor"}]}'
        'Documents/My Games/Starfield/ControlMap_Custom.txt' = 'Unrelated Controls sentinel'
        'Data/SFSE/Plugins/OSF/Settings/schemas/othermod.json' = '{"groups":{}}'
    }
    foreach ($item in $sentinels.GetEnumerator()) {
        $path = Join-Path $Directory $item.Key
        [IO.Directory]::CreateDirectory((Split-Path $path -Parent)) | Out-Null
        [IO.File]::WriteAllText($path, $item.Value)
    }
    $before = @{}
    foreach ($name in $sentinels.Keys) { $before[$name] = (Get-FileHash -LiteralPath (Join-Path $Directory $name)).Hash }
    [IO.Compression.ZipFile]::ExtractToDirectory($Archive, $data, $true)
    foreach ($name in $sentinels.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Directory $name)).Hash -cne $before[$name]) { throw "Reinstall replaced player state: $name" }
    }
    return $before
}
