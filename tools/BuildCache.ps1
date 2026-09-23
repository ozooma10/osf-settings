# Content fingerprints for small authored inputs; metadata for external toolchains
# and game archives avoids rereading large, unchanged dependencies on every build.
function Get-BuildFingerprint {
    param([string[]]$Files, [string[]]$MetadataFiles = @(), [string[]]$Values = @())
    $parts = [Collections.Generic.List[string]]::new()
    foreach ($path in @($Files | Sort-Object -Unique)) {
        $item = Get-Item -LiteralPath $path -ErrorAction Stop
        $parts.Add("$($item.FullName):$((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash)")
    }
    foreach ($path in @($MetadataFiles | Sort-Object -Unique)) {
        $item = Get-Item -LiteralPath $path -ErrorAction Stop
        $parts.Add("$($item.FullName):$($item.Length):$($item.LastWriteTimeUtc.Ticks)")
    }
    $parts.AddRange($Values)
    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        [BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes(($parts -join "`n")))).Replace('-', '')
    } finally { $hasher.Dispose() }
}

function Test-BuildCache {
    param([string]$Stamp, [string]$Fingerprint, [string[]]$Outputs)
    if (-not (Test-Path -LiteralPath $Stamp)) { return $false }
    foreach ($path in $Outputs) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $false } }
    # Check output contents too: an edited or partially written output is not fresh.
    $expected = Get-BuildFingerprint -Files $Outputs -Values @($Fingerprint)
    (Get-Content -LiteralPath $Stamp -Raw).Trim() -eq $expected
}

function Save-BuildCache {
    param([string]$Stamp, [string]$Fingerprint, [string[]]$Outputs)
    Get-BuildFingerprint -Files $Outputs -Values @($Fingerprint) | Set-Content -LiteralPath $Stamp
}
