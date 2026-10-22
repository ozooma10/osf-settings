[CmdletBinding()]
param(
    [switch]$Examples,
    [switch]$TestHarness,
    [switch]$Force,
    [string]$Compiler = 'C:\Program Files (x86)\Steam\steamapps\common\Starfield\Tools\Papyrus Compiler\PapyrusCompiler.exe',
    [string]$Imports = 'C:\Modding\Starfield\PapyrusSource'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot/BuildCache.ps1"
$source = Join-Path $repo 'data\Scripts\Source'
$output = Join-Path $repo 'build\papyrus'
$flags = Join-Path $Imports 'Starfield_Papyrus_Flags.flg'
if (-not (Test-Path -LiteralPath $Compiler) -or -not (Test-Path -LiteralPath $flags)) {
    throw 'Papyrus compiler or vanilla imports missing. Supply -Compiler and -Imports.'
}
New-Item -ItemType Directory -Force -Path $output | Out-Null
# The shipped script rebuilds only when it, this script, or the toolchain changed.
$pex = Join-Path $output 'OSFSettings.pex'
$stamp = Join-Path $output 'OSFSettings.stamp'
$sources = @($PSCommandPath, "$PSScriptRoot/BuildCache.ps1") + @(Get-ChildItem -LiteralPath $source -Filter '*.psc' -File | Select-Object -ExpandProperty FullName)
$fingerprint = Get-BuildFingerprint -Files $sources -MetadataFiles @($Compiler, $flags)
if ($Force -or -not (Test-BuildCache $stamp $fingerprint @($pex))) {
    & $Compiler $source "-i=$source;$Imports" "-o=$output" "-f=$flags" -all
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $pex)) {
        throw 'OSFSettings Papyrus compilation failed.'
    }
    Save-BuildCache $stamp $fingerprint @($pex)
} else {
    Write-Host "Up to date: $pex"
}
if ($Examples) {
    $exampleSource = Join-Path $repo 'examples\papyrus'
    $exampleOutput = Join-Path $output 'examples'
    New-Item -ItemType Directory -Force -Path $exampleOutput | Out-Null
    & $Compiler $exampleSource "-i=$exampleSource;$source;$Imports" "-o=$exampleOutput" "-f=$flags" -all
    if ($LASTEXITCODE -ne 0) { throw 'Papyrus example compilation failed.' }
}
if ($TestHarness) {
    $testSource = Join-Path $repo 'tests\harness\papyrus'
    $testOutput = Join-Path $output 'harness'
    New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
    & $Compiler $testSource "-i=$testSource;$source;$Imports" "-o=$testOutput" "-f=$flags" -all
    if ($LASTEXITCODE -ne 0) { throw 'Papyrus acceptance fixture compilation failed.' }
    & (Join-Path $repo 'tests\harness\build-fixture.ps1') -OutputDirectory $testOutput
}
