[CmdletBinding()]
param(
    [switch]$Examples,
    [string]$Compiler = 'C:\Program Files (x86)\Steam\steamapps\common\Starfield\Tools\Papyrus Compiler\PapyrusCompiler.exe',
    [string]$Imports = 'C:\Modding\Starfield\PapyrusSource'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$source = Join-Path $repo 'data\Scripts\Source'
$output = Join-Path $repo 'build\papyrus'
$flags = Join-Path $Imports 'Starfield_Papyrus_Flags.flg'
if (-not (Test-Path -LiteralPath $Compiler) -or -not (Test-Path -LiteralPath $flags)) {
    throw 'Papyrus compiler or vanilla imports missing. Supply -Compiler and -Imports.'
}
New-Item -ItemType Directory -Force -Path $output | Out-Null
& $Compiler $source "-i=$source;$Imports" "-o=$output" "-f=$flags" -all
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $output 'OSFSettings.pex'))) {
    throw 'OSFSettings Papyrus compilation failed.'
}
if ($Examples) {
    $exampleSource = Join-Path $repo 'examples\papyrus'
    $exampleOutput = Join-Path $output 'examples'
    New-Item -ItemType Directory -Force -Path $exampleOutput | Out-Null
    & $Compiler $exampleSource "-i=$exampleSource;$source;$Imports" "-o=$exampleOutput" "-f=$flags" -all
    if ($LASTEXITCODE -ne 0) { throw 'Papyrus example compilation failed.' }
}
