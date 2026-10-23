. (Join-Path $PSScriptRoot 'WorkflowCache.ps1')
. (Join-Path $PSScriptRoot 'SmokeBench.ps1')
function Invoke-SettingsContractChecks([string]$Repository, [string]$Harness, [string]$Log, [switch]$Force) {
    $cacheLog = Join-Path $Repository 'build/workflow-cache/release-contracts.log'
    $receipt = Invoke-WorkflowCached -Stamp (Join-Path $Repository 'build/workflow-cache/release-contracts.json') -Force:$Force -Fingerprint {
        Get-WorkflowFingerprint -Paths @((Get-Command git).Source) -Values @(
            (Get-SmokeSourceIdentity $Repository).sha256, (Get-SmokeSourceIdentity $Harness).sha256)
    } -Outputs { @($cacheLog) } -Action {
        if (Test-Path -LiteralPath $cacheLog) { Remove-Item -LiteralPath $cacheLog -Force }
        foreach ($test in 'smoke_bench_tests.ps1', 'release_validation_tests.ps1', 'preview_workflow_tests.ps1') {
            $arguments = @('-NoProfile','-File',(Join-Path $Repository "tests/$test"))
            if ($test -eq 'release_validation_tests.ps1') { $arguments += @('-Harness', $Harness) }
            & pwsh @arguments 2>&1 | Tee-Object -FilePath $cacheLog -Append | Out-Host
            if ($LASTEXITCODE) { throw "[failed] $test returned $LASTEXITCODE" }
        }
    }
    Copy-Item -LiteralPath $cacheLog -Destination $Log -Force
    Write-Host "Workflow contracts tested $($receipt.completedUtc)"
}
