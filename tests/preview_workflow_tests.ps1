#requires -Version 7.2
# Integration regression for the real preview entrypoint; substitutes only the
# asset builder and Ruffle process. No browser, game or native compiler is run.
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repository=Split-Path $PSScriptRoot -Parent
$scratch=Join-Path ([IO.Path]::GetTempPath()) ('osf-preview-cache-'+[guid]::NewGuid().ToString('N'))
function Check($Condition,[string]$Message){if(-not $Condition){throw $Message}}
try {
    foreach($folder in 'tools','external/ruffle'){[IO.Directory]::CreateDirectory((Join-Path $scratch $folder))|Out-Null}
    Copy-Item -LiteralPath "$repository/tools/WorkflowCache.ps1","$repository/tools/test-menu-preview.ps1" -Destination "$scratch/tools"
    Set-Content "$scratch/external/ruffle/ruffle.exe" 'fixture executable'
    @'
param([switch]$BuildOnly,[switch]$Design,[switch]$Issues,[switch]$LargeText,[string]$InterfaceArchive,[switch]$Force)
$root=Split-Path $PSScriptRoot -Parent
$output=Join-Path $root $(if($LargeText){'build/preview/large'}else{'build/preview/normal'})
[IO.Directory]::CreateDirectory($output)|Out-Null
foreach($name in 'PreviewHost.swf','menu.swf','library.swf'){
    if(-not(Test-Path "$output/$name")){Set-Content "$output/$name" "$name $LargeText"}
}
Set-Content "$output/preview.xml" '<preview><libraries><library url="library.swf" /></libraries></preview>'
'@ | Set-Content "$scratch/tools/preview-menu.ps1"
    $global:osfPreviewTestLaunches=[Collections.Generic.List[string]]::new()
    $global:osfPreviewTestFail=$false
    function Start-Process {
        param($FilePath,$WorkingDirectory,$WindowStyle,$ArgumentList,$RedirectStandardOutput,$RedirectStandardError,[switch]$PassThru)
        $global:osfPreviewTestLaunches.Add("$WorkingDirectory|$($ArgumentList -join ',')")
        $marker=if($global:osfPreviewTestFail){'FAIL'}else{'PASS'}
        Set-Content -LiteralPath $RedirectStandardOutput -Value "[preview-png:case] cG5n`n[verify] $marker"
        Set-Content -LiteralPath $RedirectStandardError -Value ''
        $process=[pscustomobject]@{HasExited=$true}
        $process|Add-Member -MemberType ScriptMethod -Name Dispose -Value {}
        $process|Add-Member -MemberType ScriptMethod -Name WaitForExit -Value {}
        return $process
    }
    function Preview([switch]$Large,[switch]$Bindings,[switch]$Force){
        & "$scratch/tools/test-menu-preview.ps1" -Capture -LargeText:$Large -Bindings:$Bindings -Force:$Force -ResultPath "$scratch/result.json"
        Get-Content "$scratch/result.json" -Raw|ConvertFrom-Json
    }
    $first=Preview
    $null=Preview -Large
    $null=Preview -Bindings
    $null=Preview -Large -Bindings
    Check ($global:osfPreviewTestLaunches.Count -eq 4) 'All four variants did not execute separately'
    $again=Preview
    $null=Preview -Large
    $null=Preview -Bindings
    $null=Preview -Large -Bindings
    Check ($global:osfPreviewTestLaunches.Count -eq 4) 'Unchanged variants did not reuse their separate evidence'
    Check ($first.completedUtc -eq $again.completedUtc) 'Reuse claimed a new test timestamp'
    Set-Content "$scratch/build/preview/normal/menu.swf" 'edited menu'
    $null=Preview
    Check ($global:osfPreviewTestLaunches.Count -eq 5) 'Changed movie did not invalidate the preview test'
    $null=Preview -Large
    Check ($global:osfPreviewTestLaunches.Count -eq 5) 'Normal change invalidated the large preview'
    Remove-Item "$scratch/build/preview/large/general/large-case.png"
    $null=Preview -Large
    Check ($global:osfPreviewTestLaunches.Count -eq 6) 'Deleted screenshot was reused'
    $global:osfPreviewTestFail=$true
    try {$null=Preview -Force;throw 'Failed preview accepted'}catch{Check ($_.Exception.Message -like 'Preview checks failed*') 'Wrong preview failure'}
    Check (-not(Test-Path "$scratch/build/preview/normal/general.cache.json")) 'Failed preview retained a cached pass'
    $global:osfPreviewTestFail=$false
    $null=Preview
    Check ($global:osfPreviewTestLaunches.Count -eq 8) 'Retry after failure borrowed old evidence'
    Check (@($global:osfPreviewTestLaunches|Where-Object {$_ -like '*-PverifyBindings=true*'}).Count -eq 2) 'Bindings assertions lost their launch flag'
    Write-Host 'PASS preview workflow: four variants, warm reuse, original timestamps, changed movies, missing captures, failure and retry.'
} finally {
    $resolved=[IO.Path]::GetFullPath($scratch)
    if(-not $resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase)){throw 'Scratch path escaped temp directory.'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
