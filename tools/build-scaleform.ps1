[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$flex = Join-Path $repo 'external\flex'
$jdk = Join-Path $repo 'external\temurin8'
$compiler = Join-Path $flex 'bin\mxmlc.bat'
$java = Join-Path $jdk 'bin\java.exe'
$player = Join-Path $flex 'frameworks\libs\player\10.3\playerglobal.swc'
if (-not (Test-Path $compiler) -or -not (Test-Path $java) -or -not (Test-Path $player)) {
    throw 'Scaleform toolchain missing. Run pwsh tools/setup-scaleform.ps1.'
}
function Add-GameLibraries([string]$InputPath, [string]$OutputPath, [string]$SettingsLibrary) {
    # Vanilla PauseMenu uses zero-symbol ImportAssets2 tags to make the exported
    # classes in these libraries available before its document class executes.
    # Flex cannot author that Scaleform linkage. Add it to the compiled SWF here.
    $source = [IO.File]::ReadAllBytes($InputPath)
    $signature = [Text.Encoding]::ASCII.GetString($source, 0, 3)
    $payload = [IO.MemoryStream]::new()
    if ($signature -eq 'CWS') {
        $compressed = [IO.MemoryStream]::new($source, 8, $source.Length - 8)
        $inflater = [IO.Compression.ZLibStream]::new($compressed, [IO.Compression.CompressionMode]::Decompress)
        try { $inflater.CopyTo($payload) } finally { $inflater.Dispose(); $compressed.Dispose() }
    } elseif ($signature -eq 'FWS') {
        $payload.Write($source, 8, $source.Length - 8)
    } else { throw "Unsupported SWF signature: $signature" }
    $body = $payload.ToArray(); $payload.Dispose()
    if ($body.Length + 8 -ne [BitConverter]::ToUInt32($source, 4)) { throw 'Invalid SWF length' }
    $rectBytes = [int][Math]::Ceiling((5 + 4 * ($body[0] -shr 3)) / 8.0)
    $firstTag = $rectBytes + 4 # frame rate and frame count follow RECT
    $record = [BitConverter]::ToUInt16($body, $firstTag)
    if (($record -shr 6) -ne 69 -or ($record -band 63) -ne 4) { throw 'Expected leading FileAttributes tag' }
    $insertAt = $firstTag + 6
    $imports = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($imports)
    foreach ($library in @('fonts_en.swf', $SettingsLibrary)) {
        $url = [Text.Encoding]::ASCII.GetBytes($library)
        $writer.Write([uint16]((71 -shl 6) -bor ($url.Length + 5)))
        $writer.Write($url); $writer.Write([byte]0)
        $writer.Write([byte]1); $writer.Write([byte]0); $writer.Write([uint16]0)
    }
    $writer.Flush(); $importBytes = $imports.ToArray(); $writer.Dispose(); $imports.Dispose()
    $outputStream = [IO.File]::Create($OutputPath)
    $outputWriter = [IO.BinaryWriter]::new($outputStream)
    try {
        $outputWriter.Write([Text.Encoding]::ASCII.GetBytes('FWS'))
        $outputWriter.Write($source[3])
        $outputWriter.Write([uint32](8 + $body.Length + $importBytes.Length))
        $outputWriter.Write($body, 0, $insertAt)
        $outputWriter.Write($importBytes)
        $outputWriter.Write($body, $insertAt, $body.Length - $insertAt)
    } finally { $outputWriter.Dispose(); $outputStream.Dispose() }
    Write-Host "Built $OutputPath (imports $SettingsLibrary and shared game fonts)"
}

$outputDirectory = Join-Path $repo 'build\scaleform'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$env:JAVA_HOME = $jdk
foreach ($large in @($false, $true)) {
    $suffix = if ($large) { '_LRG' } else { '' }
    $raw = Join-Path $outputDirectory "OSFSettingsMenu$suffix.raw.swf"
    $output = Join-Path $outputDirectory "OSFSettingsMenu$suffix.swf"
    $largeDefine = if ($large) { 'true' } else { 'false' }
    & $compiler '-load-config=' '-target-player=10.3' '-swf-version=12' "-external-library-path+=$player" '-use-network=false' '-debug=false' '-optimize=true' "-define=CONFIG::largeText,$largeDefine" "-output=$raw" (Join-Path $repo 'scaleform\src\OSFSettingsMenu.as')
    if ($LASTEXITCODE -ne 0) { throw "mxmlc failed with exit code $LASTEXITCODE" }
    if (-not (Test-Path $raw)) { throw "mxmlc did not create $raw" }
    Add-GameLibraries $raw $output "SettingsPanel$suffix.swf"
}
