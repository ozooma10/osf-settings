#requires -Version 7.2
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../tools/SmokeBench.ps1')
$scratch = Join-Path $PSScriptRoot ('../build/smoke-selftest/' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($scratch) | Out-Null
$script:checks = 0
function Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Self-test failed: $Message" }
    $script:checks++; Write-Host "PASS $Message"
}
function Reject([scriptblock]$Body, [string]$Message = 'Invalid runtime receipt rejected') {
    $rejected = $false
    try { & $Body } catch { $rejected = $true }
    Check $rejected $Message
}
Test-SmokeNativeSummary "`e[32m100%`e[0m tests passed, 0 test(s) failed out of 22, spent 1s" 22
Check $true 'ANSI-colored XMake summary accepted'
Reject { Test-SmokeNativeSummary 'nothing to test' 22 } 'Missing native test summary rejected'
Reject { Test-SmokeNativeSummary '100% tests passed, 0 test(s) failed out of 0, spent 0s' 22 } 'Zero executed native tests rejected'
Reject { Test-SmokeNativeSummary '100% tests passed, 0 test(s) failed out of 21, spent 1s' 22 } 'Incomplete native test count rejected'
$null = Invoke-SmokeGit $scratch @('init','-q')
'build/' | Set-Content (Join-Path $scratch '.gitignore')
'one' | Set-Content (Join-Path $scratch 'source with spaces.cpp')
$null = Invoke-SmokeGit $scratch @('add','.')
$null = Invoke-SmokeGit $scratch @('-c','user.name=selftest','-c','user.email=selftest@invalid','commit','-qm','fixture')
$before = Get-SmokeSourceIdentity $scratch
Check (-not $before.dirty -and $before.sha256 -eq (Get-SmokeSourceIdentity $scratch).sha256) 'Clean source identity is stable'
'two' | Set-Content (Join-Path $scratch 'source with spaces.cpp')
$edited = Get-SmokeSourceIdentity $scratch
Check ($edited.dirty -and $before.sha256 -ne $edited.sha256) 'Uncommitted content changes identity'
'new' | Set-Content (Join-Path $scratch 'new.cpp')
$added = Get-SmokeSourceIdentity $scratch
Check ($added.sha256 -ne $edited.sha256) 'Untracked source changes identity'
[IO.Directory]::CreateDirectory((Join-Path $scratch 'build')) | Out-Null
'output' | Set-Content (Join-Path $scratch 'build/output.txt')
Check ($added.sha256 -eq (Get-SmokeSourceIdentity $scratch).sha256) 'Ignored build output does not change identity'
Remove-Item -LiteralPath (Join-Path $scratch 'source with spaces.cpp')
Check ($added.sha256 -ne (Get-SmokeSourceIdentity $scratch).sha256) 'Deleted tracked source changes identity'
$now = [DateTimeOffset]::UtcNow
$result = Join-Path $scratch 'case.json'
@{ scenario='SettingsSmoke'; outcome='passed'; startedAt=$now.AddSeconds(1); finishedAt=$now.AddSeconds(2) } | ConvertTo-Json | Set-Content $result
$receipt = @{ automatedPassed=$true; completeSuite=$true; builtFromSource=$true; source=@{sha256='source'}; selectedCases=@('one')
    runs=@(@{name='one'; outcome='passed'; exitCode=0; result=$result; resultSha256=(Get-FileHash $result).Hash; startedAt=$now; finishedAt=$now.AddSeconds(3)}) }
Test-SmokeRuntimeReceipt $receipt @('one') 'source'
Check $true 'Complete passing receipt accepted'
Reject { Test-SmokeRuntimeReceipt $receipt @('one','two') 'source' }
Reject { Test-SmokeRuntimeReceipt $receipt @('one') 'other-source' }
$receipt.completeSuite = $false
Reject { Test-SmokeRuntimeReceipt $receipt @('one') 'source' }
$receipt.completeSuite = $true; $receipt.automatedPassed = 'false'
Reject { Test-SmokeRuntimeReceipt $receipt @('one') 'source' }
$receipt.automatedPassed = $true; $receipt.runs[0].resultSha256 = 'altered-receipt'
Reject { Test-SmokeRuntimeReceipt $receipt @('one') 'source' }
$receipt.runs[0].resultSha256 = (Get-FileHash $result).Hash
$receipt.builtFromSource = $false
Reject { Test-SmokeRuntimeReceipt $receipt @('one') 'source' } 'Unverified staged binaries rejected'
$receipt.builtFromSource = $true
foreach ($fault in @('stale','unfinished','wrong-scenario','wrong-outcome','future','reversed')) {
    $evidence = @{ scenario='SettingsSmoke'; outcome='passed'; startedAt=$now.AddSeconds(1).ToString('o'); finishedAt=$now.AddSeconds(2).ToString('o') }
    switch ($fault) {
        stale { $evidence.startedAt = $now.AddDays(-1).ToString('o') }
        unfinished { $evidence.finishedAt = $null }
        wrong-scenario { $evidence.scenario = 'Other' }
        wrong-outcome { $evidence.outcome = 'failed' }
        future { $evidence.finishedAt = $now.AddDays(1).ToString('o') }
        reversed { $evidence.finishedAt = $now.ToString('o') }
    }
    $evidence | ConvertTo-Json | Set-Content $result
    $receipt.runs[0].resultSha256 = (Get-FileHash $result).Hash
    Reject { Test-SmokeRuntimeReceipt $receipt @('one') 'source' } "$fault scenario evidence rejected even with a matching hash"
}
@{ scenario='SettingsSmoke'; outcome='passed'; startedAt=$now.AddSeconds(1); finishedAt=$now.AddSeconds(2) } | ConvertTo-Json | Set-Content $result
$receipt.runs[0].resultSha256 = (Get-FileHash $result).Hash
Test-SmokeRuntimeReceipt $receipt @('one') 'source'
$receipt.runs += $receipt.runs[0]
Reject { Test-SmokeRuntimeReceipt $receipt @('one') 'source' } 'Duplicate case rejected'
$receipt.runs[1] = $receipt.runs[0].Clone(); $receipt.runs[1].name = 'two'; $receipt.selectedCases = @('one','two')
Reject { Test-SmokeRuntimeReceipt $receipt @('one','two') 'source' } 'Reusing one result file for different cases rejected'
$report = @{ outcome='blocked'; scope='offline'; completeAutomatedSuite=$false; coverage=@(); unverified=@('hardware')
    stages=@(@{name='compile & test'; status='failed'; seconds=1.5; message='bad <value>'; log='test.log'},
        @{name='game'; status='not-run'; seconds=0; message='skipped'; log=$null}) }
Save-SmokeReport $report $scratch
$xml = [xml](Get-Content (Join-Path $scratch 'junit.xml') -Raw)
Check ($xml.testsuite.failures -eq '1' -and $xml.testsuite.skipped -eq '1') 'JUnit preserves failures and skipped stages'
Check ($xml.testsuite.testcase[0].failure -eq 'bad <value>') 'JUnit escapes diagnostic text'
Write-Host "$script:checks smoke bench checks passed. Evidence: $scratch"
