# A single start-enabled quest with a forced player alias, no overrides or gameplay content.
# Record layout: TES5Edit dev-4.1.5 Core/wbDefinitionsSF1.pas (QUST and VMAD).
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
function New-Bytes([scriptblock]$Write) {
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    try { & $Write $writer; return ,$stream.ToArray() } finally { $writer.Dispose(); $stream.Dispose() }
}
function Write-Text($Writer, [string]$Text) { $Writer.Write([Text.Encoding]::UTF8.GetBytes($Text)) }
function Write-LengthText($Writer, [string]$Text) {
    $data = [Text.Encoding]::UTF8.GetBytes($Text); $Writer.Write([uint16]$data.Length); $Writer.Write($data)
}
function Write-Part($Writer, [string]$Signature, [byte[]]$Body) {
    Write-Text $Writer $Signature; $Writer.Write([uint16]$Body.Length); $Writer.Write($Body)
}
function Write-Record($Writer, [string]$Signature, [uint32]$Id, [uint32]$Flags, [byte[]]$Body) {
    Write-Text $Writer $Signature; $Writer.Write([uint32]$Body.Length); $Writer.Write($Flags); $Writer.Write($Id)
    $Writer.Write([uint32]0); $Writer.Write([uint16]582); $Writer.Write([uint16]0); $Writer.Write($Body)
}
$questId = [uint32]0x01000800
$adapter = New-Bytes {
    param($w)
    $w.Write([uint16]6); $w.Write([uint16]2); $w.Write([uint16]0) # no quest script
    $w.Write([byte]3); $w.Write([uint16]0); Write-LengthText $w '' # no fragments
    $w.Write([uint16]1) # alias count
    $w.Write([uint16]0); $w.Write([int16]0); $w.Write($questId) # object format 2: alias 0 on this quest
    $w.Write([uint16]6); $w.Write([uint16]2); $w.Write([uint16]1)
    Write-LengthText $w 'OSFSettingsTestAlias'; $w.Write([byte]0); $w.Write([uint16]0)
}
$quest = New-Bytes {
    param($w)
    Write-Part $w 'EDID' ([Text.Encoding]::ASCII.GetBytes("OSFSettingsTestQuest`0"))
    Write-Part $w 'VMAD' $adapter
    Write-Part $w 'DNAM' (New-Bytes { param($b) $b.Write([uint32]0x11); $b.Write([byte]10); $b.Write([byte[]]::new(7)) })
    Write-Part $w 'NEXT' ([byte[]]::new(0))
    Write-Part $w 'ANAM' ([BitConverter]::GetBytes([uint32]1))
    Write-Part $w 'ALST' ([BitConverter]::GetBytes([uint32]0))
    Write-Part $w 'ALID' ([Text.Encoding]::ASCII.GetBytes("Player`0"))
    Write-Part $w 'FNAM' ([BitConverter]::GetBytes([uint32]0x202)) # optional; Bootstrap fills the alias on an existing save
    Write-Part $w 'ALFR' ([BitConverter]::GetBytes([uint32]0x14))
    Write-Part $w 'ALED' ([byte[]]::new(0))
}
$record = New-Bytes { param($w) Write-Record $w 'QUST' $questId 0 $quest }
$header = New-Bytes {
    param($w)
    Write-Part $w 'HEDR' (New-Bytes { param($b) $b.Write([single]0.96); $b.Write([uint32]1); $b.Write([uint32]0x801) })
    Write-Part $w 'CNAM' ([Text.Encoding]::ASCII.GetBytes("OSF Settings Test Mod`0"))
    Write-Part $w 'MAST' ([Text.Encoding]::ASCII.GetBytes("Starfield.esm`0"))
}
$plugin = New-Bytes {
    param($w)
    Write-Record $w 'TES4' 0 1 $header
    Write-Text $w 'GRUP'; $w.Write([uint32](24 + $record.Length)); Write-Text $w 'QUST'
    $w.Write([byte[]]::new(12)); $w.Write($record)
}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
[IO.File]::WriteAllBytes((Join-Path $OutputDirectory 'OSFSettingsTestMod.esm'), $plugin)
