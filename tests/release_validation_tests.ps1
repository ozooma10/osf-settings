#requires -Version 7.2
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $repo 'packaging/ReleaseValidation.ps1')
$scratch = Join-Path $repo ('build/release-selftest/' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($scratch) | Out-Null
$script:checks = 0
function Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Self-test failed: $Message" }
    $script:checks++; Write-Host "PASS $Message"
}
function Reject([scriptblock]$Operation, [string]$Pattern) {
    $errorText = ''
    try { & $Operation | Out-Null } catch { $errorText = $_.Exception.Message }
    Check ($errorText -like "*$Pattern*") "Reject $Pattern"
}
function New-Candidate([string]$Name, [string]$Fault = '') {
    $directory = Join-Path $scratch $Name
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $archive = Join-Path $directory 'candidate.zip'
    $zip = [IO.Compression.ZipFile]::Open($archive, 'Create')
    $records = @()
    try {
        $paths = @(Get-ReleasePayloadPaths)
        if ($Fault -eq 'traversal') { $paths[-1] = '../escaped.dll' }
        if ($Fault -eq 'duplicate') { $paths[-1] = $paths[0] }
        foreach ($path in $paths) {
            $payload = if ($path.EndsWith('.dll')) { 'MZ OSFSettings_RequestAPI OSFSettings_RequestLauncherAPI OSFSettings_RequestDiagnosticsAPI' }
                elseif ($path.EndsWith('.swf')) { 'FWS production fixture' }
                elseif ($path.EndsWith('.json')) { '{}' } else { 'fixture' }
            if ($Fault -eq 'test-dll' -and $path.EndsWith('.dll')) { $payload += ' OSFSettings_TestSnapshot' }
            if ($Fault -eq 'test-movie' -and $path.EndsWith('.swf')) { $payload += ' reportTestState' }
            [byte[]]$bytes = if ($path.EndsWith('.pex')) { [byte[]](0xDE,0xC0,0x57,0xFA,0) } else { [Text.Encoding]::UTF8.GetBytes($payload) }
            if ($Fault -eq 'bad-pex' -and $path.EndsWith('.pex')) { $bytes[0] = 0 }
            $entry = $zip.CreateEntry($path)
            $stream = $entry.Open()
            try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
            $records += @{ path=$path; bytes=$bytes.Length; sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)) }
        }
    } finally { $zip.Dispose() }
    if ($Fault -eq 'file-hash') { $records[0].sha256 = '0' * 64 }
    $hash = (Get-FileHash -LiteralPath $archive).Hash
    "$hash  candidate.zip" | Set-Content -LiteralPath "$archive.sha256"
    $manifest = Join-Path $directory 'candidate.manifest.json'
    @{ archive='candidate.zip'; sha256=$hash; configuration='windows/x64/releasedbg; test_harness=n'; files=$records } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifest
    return $manifest
}
$good = New-Candidate 'valid'
$candidate = Test-ReleaseArchive $good
Check ($candidate.files -eq 15) 'Valid archive has all expected payloads'
$preserved = Test-ReleaseReinstall $candidate.archive (Join-Path $scratch 'reinstall')
Check ($preserved.Count -eq 3) 'Reinstall preserves all player-state sentinels'
foreach ($fault in @(
    @('traversal','Unexpected or duplicate'), @('duplicate','Invalid payload record'),
    @('test-dll','Instrumented DLL'), @('test-movie','Development movie'),
    @('bad-pex','PEX header'), @('file-hash','Payload checksum')
)) {
    $manifest = New-Candidate $fault[0] $fault[0]
    Reject { Test-ReleaseArchive $manifest } $fault[1]
}
Add-Content -LiteralPath $candidate.archive -Value 'tampered'
Reject { Test-ReleaseArchive $good } 'archive checksum'

Write-Host "$script:checks release validation checks passed. Evidence: $scratch"
