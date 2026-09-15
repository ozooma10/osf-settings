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
function New-SwfData([scriptblock]$Content) {
    $dataStream = [IO.MemoryStream]::new()
    $dataWriter = [IO.BinaryWriter]::new($dataStream)
    try {
        & $Content $dataWriter
        return ,$dataStream.ToArray()
    } finally { $dataWriter.Dispose(); $dataStream.Dispose() }
}
function Write-SwfString([IO.BinaryWriter]$Writer, [string]$Value) {
    $Writer.Write([Text.Encoding]::ASCII.GetBytes($Value)); $Writer.Write([byte]0)
}
function Write-SwfTag([IO.BinaryWriter]$Writer, [int]$Code, [scriptblock]$Content = {}) {
    $data = New-SwfData $Content
    $Writer.Write([uint16](($Code -shl 6) -bor [Math]::Min($data.Length, 63)))
    if ($data.Length -ge 63) { $Writer.Write([uint32]$data.Length) }
    $Writer.Write($data)
}
function New-TextSymbols {
    # DefineEditText.FontClass binds the shared font at movie load, as in vanilla.
    # These symbols contain no glyphs and need no AS3 font class lookup.
    $bindings = @(
        @{ TextID = 1; SpriteID = 3; Class = 'MenuLabelField'; Font = '$MAIN_Font_Bold' }
        @{ TextID = 2; SpriteID = 4; Class = 'MenuBodyField'; Font = '$NB_Grotesk_Semibold' }
    )
    # Signed 16-bit RECT: (0, 20000, 0, 2000) twips; AS3 sets each field's size.
    $bits = '10000' + ('0' * 16) + [Convert]::ToString(20000, 2).PadLeft(16, '0') + ('0' * 16) + [Convert]::ToString(2000, 2).PadLeft(16, '0')
    $bits = $bits.PadRight([int]([Math]::Ceiling($bits.Length / 8.0) * 8), '0')
    $rect = [byte[]]::new($bits.Length / 8)
    for ($i = 0; $i -lt $rect.Length; ++$i) { $rect[$i] = [Convert]::ToByte($bits.Substring($i * 8, 8), 2) }
    return ,(New-SwfData {
        param($tags)
        foreach ($binding in $bindings) {
            Write-SwfTag $tags 37 { # DefineEditText
                param($field)
                $field.Write([uint16]$binding.TextID); $field.Write($rect)
                # HasText, ReadOnly, HasTextColor, HasFontClass, HasLayout, NoSelect, UseOutlines.
                $field.Write([byte]0x8C); $field.Write([byte]0xB1)
                Write-SwfString $field $binding.Font
                $field.Write([uint16]560); $field.Write([uint32]::MaxValue)
                $field.Write([byte]0) # left aligned
                for ($j = 0; $j -lt 4; ++$j) { $field.Write([uint16]0) } # margins, indent, leading
                Write-SwfString $field ''; Write-SwfString $field '' # variable and initial text
            }
            Write-SwfTag $tags 39 { # DefineSprite
                param($sprite)
                $sprite.Write([uint16]$binding.SpriteID); $sprite.Write([uint16]1)
                Write-SwfTag $sprite 26 { # PlaceObject2
                    param($place)
                    $place.Write([byte]0x26); $place.Write([uint16]1); $place.Write([uint16]$binding.TextID)
                    $place.Write([byte]0) # identity matrix
                    Write-SwfString $place 'textField'
                }
                Write-SwfTag $sprite 1; Write-SwfTag $sprite 0 # ShowFrame, End
            }
        }
        Write-SwfTag $tags 76 { # SymbolClass
            param($symbols)
            $symbols.Write([uint16]$bindings.Count)
            foreach ($binding in $bindings) {
                $symbols.Write([uint16]$binding.SpriteID)
                Write-SwfString $symbols $binding.Class
            }
        }
    })
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
    $frameAt = -1
    for ($offset = $firstTag; $offset -lt $body.Length;) {
        $start = $offset; $record = [BitConverter]::ToUInt16($body, $offset); $offset += 2
        $code = $record -shr 6; $length = $record -band 63
        if ($length -eq 63) { $length = [BitConverter]::ToUInt32($body, $offset); $offset += 4 }
        if ($offset + $length -gt $body.Length) { throw 'Truncated SWF tag' }
        # Flex currently emits code only. Reject added assets rather than collide
        # with their character IDs when adding our four text/sprite definitions.
        if ($code -notin @(0, 1, 9, 41, 43, 65, 69, 76, 77, 82)) { throw "Unexpected compiled SWF tag: $code" }
        if ($code -eq 1 -and $frameAt -lt 0) { $frameAt = $start }
        $offset += $length
    }
    if ($frameAt -lt 0) { throw 'Compiled SWF has no ShowFrame' }
    $textSymbols = New-TextSymbols
    $importBytes = New-SwfData {
        param($imports)
        foreach ($library in @('fonts_en.swf', $SettingsLibrary)) {
            Write-SwfTag $imports 71 { # ImportAssets2
                param($import)
                Write-SwfString $import $library
                $import.Write([byte]1); $import.Write([byte]0); $import.Write([uint16]0)
            }
        }
    }
    foreach ($destination in @($InputPath, $OutputPath)) {
        # The preview uses the same authored symbols, with imports loaded by its host.
        $importsForFile = if ($destination -eq $OutputPath) { $importBytes } else { [byte[]]@() }
        $outputStream = [IO.File]::Create($destination)
        $outputWriter = [IO.BinaryWriter]::new($outputStream)
        try {
            $outputWriter.Write([Text.Encoding]::ASCII.GetBytes('FWS'))
            $outputWriter.Write($source[3])
            $outputWriter.Write([uint32](8 + $body.Length + $importsForFile.Length + $textSymbols.Length))
            $outputWriter.Write($body, 0, $insertAt)
            if ($importsForFile.Length) { $outputWriter.Write([byte[]]$importsForFile) }
            $outputWriter.Write($body, $insertAt, $frameAt - $insertAt)
            $outputWriter.Write($textSymbols)
            $outputWriter.Write($body, $frameAt, $body.Length - $frameAt)
        } finally { $outputWriter.Dispose(); $outputStream.Dispose() }
    }
    Write-Host "Built $OutputPath (authored text; imports $SettingsLibrary and shared game fonts)"
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
