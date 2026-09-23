[CmdletBinding()]
param([switch]$Preview)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$external = Join-Path $repo 'external'
$flex = Join-Path $external 'flex'
$jdk = Join-Path $external 'temurin8'
$cache = Join-Path $external '.downloads'
New-Item -ItemType Directory -Force -Path $external,$cache | Out-Null

function Fetch([string]$Url, [string]$Path, [string]$Sha256) {
    if (-not (Test-Path $Path)) {
        curl.exe -L --fail --show-error -o $Path $Url
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $Url" }
    }
    $actual = (Get-FileHash $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Sha256) { throw "SHA256 mismatch for $Path`nexpected $Sha256`nactual   $actual" }
}

if (-not (Test-Path (Join-Path $flex 'bin\mxmlc.bat'))) {
    $flexZip = Join-Path $cache 'apache-flex-sdk-4.16.1-bin.zip'
    Fetch 'https://archive.apache.org/dist/flex/4.16.1/binaries/apache-flex-sdk-4.16.1-bin.zip' $flexZip '757aa19299c8a9c8af0901c1ae35f97fa94b7af0b0a9abc2bab04fe61d756e8b'
    Expand-Archive -LiteralPath $flexZip -DestinationPath $flex -Force
}
if (-not (Test-Path (Join-Path $jdk 'bin\java.exe'))) {
    $jdkZip = Join-Path $cache 'OpenJDK8U-jdk_x64_windows_hotspot_8u462b08.zip'
    Fetch 'https://github.com/adoptium/temurin8-binaries/releases/download/jdk8u462-b08/OpenJDK8U-jdk_x64_windows_hotspot_8u462b08.zip' $jdkZip 'c6c2d8e6c2cbf0b4ddb707e6edb9a00d8a7b04e9ed6dda7e62d43dc4d83d342c'
    $unpack = Join-Path $cache 'temurin8-unpack'
    Expand-Archive -LiteralPath $jdkZip -DestinationPath $unpack -Force
    $root = Get-ChildItem -LiteralPath $unpack -Directory | Select-Object -First 1
    if (-not $root) { throw 'Temurin archive had no root directory.' }
    New-Item -ItemType Directory -Force -Path $jdk | Out-Null
    Get-ChildItem -LiteralPath $root.FullName | Copy-Item -Destination $jdk -Recurse -Force
}
$playerDest = Join-Path $flex 'frameworks\libs\player\10.3\playerglobal.swc'
if (-not (Test-Path -LiteralPath $playerDest)) {
    $player = Join-Path $cache 'playerglobal-10.3.swc'
    Fetch 'https://raw.githubusercontent.com/nexussays/playerglobal/master/10.3/playerglobal.swc' $player 'ba7573783a5b1754966721d163471e50675d52a7a3830de15d7707a08e606b7a'
    New-Item -ItemType Directory -Force -Path (Split-Path $playerDest -Parent) | Out-Null
    Copy-Item -LiteralPath $player -Destination $playerDest -Force
}
Write-Host 'Scaleform toolchain ready: Apache Flex 4.16.1 + Temurin 8u462 + playerglobal 10.3'

if ($Preview) {
    $ruffle = Join-Path $external 'ruffle'
    if (-not (Test-Path -LiteralPath (Join-Path $ruffle 'ruffle.exe'))) {
        $archive = Join-Path $cache 'ruffle-0.6.0-windows-x86_64.zip'
        Fetch 'https://github.com/ruffle-rs/ruffle/releases/download/v0.6.0/ruffle-0.6.0-windows-x86_64.zip' $archive '4d151b83054ba2b9f6d4e422babee9e27c0d187634e50359d4857d315d6b52c6'
        Expand-Archive -LiteralPath $archive -DestinationPath $ruffle -Force
    }
    Write-Host 'Portable Ruffle 0.6.0 ready.'
}
