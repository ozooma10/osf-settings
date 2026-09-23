# Shared by packaging and the final-release runner. No deployment or game actions.
Set-StrictMode -Version Latest

function Get-ReleaseSourceIdentity([string]$Repository) {
    $repository = [IO.Path]::GetFullPath($Repository)
    $lines = [Collections.Generic.List[string]]::new()
    # Include working files, not just HEAD: a dirty candidate is allowed but must
    # remain byte-for-byte unchanged throughout packaging and runtime validation.
    foreach ($relative in @('', 'lib/commonlibsf', 'lib/commonlibsf/lib/commonlib-shared')) {
        $directory = Join-Path $repository $relative
        $revision = & git -C $directory rev-parse HEAD
        if ($LASTEXITCODE) { throw "Cannot identify source repository: $directory" }
        $lines.Add("$relative HEAD $revision")
        $paths = @(& git -C $directory ls-files --cached --others --exclude-standard)
        if ($LASTEXITCODE) { throw "Cannot enumerate source repository: $directory" }
        foreach ($path in ($paths | Sort-Object -Unique -CaseSensitive)) {
            $full = Join-Path $directory $path
            if (Test-Path -LiteralPath $full -PathType Container) { continue } # gitlink; handled above
            $hash = if (Test-Path -LiteralPath $full -PathType Leaf) { (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash } else { 'DELETED' }
            $lines.Add("$relative/$path $hash")
        }
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    return [ordered]@{ algorithm = 'sha256-working-files-v1'; sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)); files = $lines.Count }
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
    if ($manifest.configuration -cne 'windows/x64/releasedbg; test_harness=n') { throw 'Candidate is not a production configuration.' }
    if ([IO.Path]::GetFileName($manifest.archive) -cne $manifest.archive -or $manifest.archive.Contains('\')) { throw 'Invalid archive filename in manifest.' }
    $archive = Join-Path (Split-Path ([IO.Path]::GetFullPath($ManifestPath)) -Parent) $manifest.archive
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    if ($hash -cne $manifest.sha256) { throw 'Candidate archive checksum mismatch.' }
    $checksum = (Get-Content -LiteralPath "$archive.sha256" -Raw).Trim()
    if ($checksum -cne "$hash  $($manifest.archive)") { throw 'Checksum sidecar mismatch.' }
    if ($Repository) {
        $identity = Get-ReleaseSourceIdentity $Repository
        if (-not $manifest.ContainsKey('sourceIdentity') -or $identity.sha256 -cne $manifest.sourceIdentity.sha256) {
            throw 'Candidate source differs from the current checkout; build a new candidate.'
        }
    }
    $allowed = @(Get-ReleasePayloadPaths)
    if ($manifest.files.Count -ne $allowed.Count) { throw 'Manifest payload count mismatch.' }
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    try {
        if ($zip.Entries.Count -ne $allowed.Count) { throw 'Archive payload count mismatch.' }
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName
            # Exact allowlist rejects traversal, absolute paths, extra fixtures,
            # case aliases and alternate streams before anything is extracted.
            if ($name -cnotin $allowed -or -not $seen.Add($name)) { throw "Unexpected or duplicate archive entry: $name" }
            $records = @($manifest.files | Where-Object path -CEQ $name)
            if ($records.Count -ne 1 -or $entry.Length -le 0 -or $entry.Length -ne $records[0].bytes -or $entry.Length -gt 32MB) { throw "Invalid payload record: $name" }
            $stream = $entry.Open()
            $buffer = [IO.MemoryStream]::new()
            try { $stream.CopyTo($buffer); $bytes = $buffer.ToArray() } finally { $stream.Dispose(); $buffer.Dispose() }
            if ([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)) -cne $records[0].sha256) { throw "Payload checksum mismatch: $name" }
            if ($name.EndsWith('.dll')) {
                $text = [Text.Encoding]::ASCII.GetString($bytes)
                if ($bytes.Length -lt 2 -or $text.Substring(0, 2) -cne 'MZ') { throw 'Invalid production DLL.' }
                foreach ($symbol in @('OSFSettings_RequestAPI', 'OSFSettings_RequestLauncherAPI', 'OSFSettings_RequestDiagnosticsAPI')) {
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
    # A new disposable Data tree only. This checks archive overwrite behavior;
    # it is deliberately not reported as a game or mod-manager upgrade test.
    if (Test-Path -LiteralPath $Directory) { throw 'Reinstall test requires a new directory.' }
    [IO.Compression.ZipFile]::ExtractToDirectory($Archive, $Directory)
    $sentinels = @{
        'SFSE/Plugins/OSF/Settings/values/osfreleaseprobe.json' = '{"values":{"enabled":true,"caption":"Preserve me"}}'
        'SFSE/Plugins/OSF/Settings/internal.json' = '{"recentLaunchers":[{"mod":"probe","id":"editor"}]}'
        'ControlMap_Custom.txt' = 'Unrelated Controls sentinel'
    }
    foreach ($item in $sentinels.GetEnumerator()) {
        $path = Join-Path $Directory $item.Key
        [IO.Directory]::CreateDirectory((Split-Path $path -Parent)) | Out-Null
        [IO.File]::WriteAllText($path, $item.Value)
    }
    $before = @{}
    foreach ($name in $sentinels.Keys) { $before[$name] = (Get-FileHash -LiteralPath (Join-Path $Directory $name)).Hash }
    [IO.Compression.ZipFile]::ExtractToDirectory($Archive, $Directory, $true)
    foreach ($name in $sentinels.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Directory $name)).Hash -cne $before[$name]) { throw "Reinstall replaced player state: $name" }
    }
    return $before
}
