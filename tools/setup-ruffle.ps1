[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $repo 'external/ruffle'
$cache = Join-Path $repo 'external/.downloads'
$archive = Join-Path $cache 'ruffle-0.6.0-windows-x86_64.zip'
New-Item -ItemType Directory -Force -Path $destination,$cache | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    curl.exe -L --fail --show-error -o $archive 'https://github.com/ruffle-rs/ruffle/releases/download/v0.6.0/ruffle-0.6.0-windows-x86_64.zip'
    if ($LASTEXITCODE -ne 0) { throw 'Ruffle download failed.' }
}
$expected = '4d151b83054ba2b9f6d4e422babee9e27c0d187634e50359d4857d315d6b52c6'
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
    throw "SHA256 mismatch for $archive"
}
Expand-Archive -LiteralPath $archive -DestinationPath $destination -Force
Write-Host 'Portable Ruffle 0.6.0 is ready. No system installation is required.'
