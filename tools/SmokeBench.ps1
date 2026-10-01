# Shared smoke-bench evidence helpers. No build, deployment, or game actions.
Set-StrictMode -Version Latest

function Invoke-SmokeGit([string]$Repository, [string[]]$Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new('git')
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in (@('-C', $Repository) + $Arguments)) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $errors = $process.StandardError.ReadToEndAsync()
        $output = $process.StandardOutput.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode) { throw "Git failed in ${Repository}: $($errors.GetAwaiter().GetResult())" }
        return $output
    } finally { $process.Dispose() }
}

function Get-SmokeSourceIdentity([string]$Repository) {
    $Repository = [IO.Path]::GetFullPath($Repository)
    $revision = (Invoke-SmokeGit $Repository @('rev-parse', 'HEAD')).Trim()
    $files = [Collections.Generic.List[object]]::new()
    # NUL delimiters preserve spaces, Unicode and unusual filenames. Include
    # untracked sources and deleted tracked paths; ignored build output is excluded.
    $paths = (Invoke-SmokeGit $Repository @('ls-files', '-z', '--cached', '--others', '--exclude-standard', '--deduplicate')).Split([char]0, [StringSplitOptions]::RemoveEmptyEntries)
    [Array]::Sort($paths, [StringComparer]::Ordinal)
    foreach ($relative in $paths) {
        $path = Join-Path $Repository $relative
        if (Test-Path -LiteralPath $path -PathType Container) {
            # Gitlinks are directories in ls-files. Hash their actual working
            # trees too, so an edited CommonLib cannot borrow old test evidence.
            $child = Get-SmokeSourceIdentity $path
            $files.Add([ordered]@{ path=$relative; sha256=$child.sha256; revision=$child.revision })
        } else {
            $hash = if (Test-Path -LiteralPath $path -PathType Leaf) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } else { 'deleted' }
            $files.Add([ordered]@{ path=$relative; sha256=$hash })
        }
    }
    $bytes = [Text.Encoding]::UTF8.GetBytes((ConvertTo-Json -InputObject @($revision, $files.ToArray()) -Depth 10 -Compress))
    return [ordered]@{
        repository=$Repository; revision=$revision
        sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
        dirty=-not [string]::IsNullOrWhiteSpace((Invoke-SmokeGit $Repository @('status', '--porcelain=v1', '--untracked-files=all', '--ignore-submodules=none')))
        files=$files.ToArray()
    }
}

function Test-SmokeNativeSummary([string]$Log, [int]$ExpectedCount) {
    $plain = [regex]::Replace($Log, '\x1B\[[0-?]*[ -/]*[@-~]', '')
    $summary = [regex]::Match($plain, '(?m)^100% tests passed, 0 test\(s\) failed out of (\d+),')
    if (-not $summary.Success -or [int]$summary.Groups[1].Value -ne $ExpectedCount) {
        throw '[blocked] XMake did not report the expected number of executed passing tests.'
    }
}

function Test-SmokeCoverage($Coverage, [string[]]$NativeSuites, [string[]]$PreviewVariants, [string[]]$RuntimeCases) {
    if (-not @($Coverage).Count) { throw '[failed] Feature coverage inventory is empty.' }
    $ids = @()
    foreach ($feature in $Coverage) {
        if (-not $feature.id -or -not $feature.feature -or $feature.id -in $ids) { throw '[failed] Feature coverage needs unique IDs and descriptions.' }
        $ids += $feature.id
        $count = 0
        foreach ($kind in @('native','preview','runtime')) {
            $entries = @($feature.$kind)
            if (@($entries | Where-Object { $_ -isnot [string] -or [string]::IsNullOrWhiteSpace($_) }).Count -or
                @($entries | Sort-Object -Unique).Count -ne $entries.Count) { throw "[failed] Invalid or duplicate $kind coverage for $($feature.id)." }
            $count += $entries.Count
        }
        if (-not $count) { throw "[failed] Feature $($feature.id) has no automated coverage." }
    }
    foreach ($inventory in @(@{kind='native'; expected=$NativeSuites}, @{kind='preview'; expected=$PreviewVariants}, @{kind='runtime'; expected=$RuntimeCases})) {
        $expected = @($inventory.expected)
        # Offline use without a harness may still validate native/preview coverage.
        if ($inventory.kind -eq 'runtime' -and -not $expected.Count) { continue }
        if (-not $expected.Count -or @($expected | Sort-Object -Unique).Count -ne $expected.Count) { throw "[failed] Empty or duplicate $($inventory.kind) test inventory." }
        $mapped = @($Coverage | ForEach-Object { $_.($inventory.kind) } | Sort-Object -Unique)
        if (-not $mapped.Count -or @(Compare-Object $expected $mapped).Count) { throw "[failed] $($inventory.kind) test inventory and smoke-coverage.json disagree." }
    }
}

function Get-SmokeFeatureResults($Report) {
    foreach ($feature in $Report.coverage) {
        $result = [ordered]@{ id=$feature.id; feature=$feature.feature }
        foreach ($kind in @('native','preview','runtime')) {
            $references = @($feature.$kind)
            $stages = @(if ($kind -eq 'preview') { $references | ForEach-Object { "preview-$_" } } else { $kind })
            $statuses = @($Report.stages | Where-Object name -in $stages | ForEach-Object status)
            $result[$kind] = if (-not $references.Count) { 'not-applicable' }
                elseif ('failed' -in $statuses) { 'failed' }
                elseif ('blocked' -in $statuses) { 'blocked' }
                elseif ($statuses.Count -eq $stages.Count -and @($statuses | Where-Object { $_ -ne 'passed' }).Count -eq 0) { 'passed' }
                else { 'not-run' }
        }
        $result
    }
}

function Read-SmokeCaseResult([string]$Path, [DateTimeOffset]$StartedAt, [DateTimeOffset]$FinishedAt, [int]$ExitCode) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing scenario receipt: $Path" }
    $result = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $expected = switch ($ExitCode) { 0 { 'passed' } 1 { 'failed' } 2 { 'blocked' } default { throw "Unexpected scenario exit code: $ExitCode" } }
    if ($result.scenario -cne 'SettingsSmoke' -or $result.outcome -cne $expected) { throw 'Scenario receipt disagrees with the requested scenario or exit code.' }
    # ConvertFrom-Json may return DateTime objects; Parse would stringify them
    # with local formatting, losing the offset and fractional seconds.
    $start = [DateTimeOffset]$result.startedAt
    $finish = [DateTimeOffset]$result.finishedAt
    if ($start -lt $StartedAt -or $finish -lt $start -or $finish -gt $FinishedAt) { throw 'Scenario receipt is stale or has invalid timestamps.' }
    return $result
}

function Test-SmokeRuntimeReceipt($Receipt, [string[]]$ExpectedCases, [string]$SourceHash) {
    if ($Receipt.automatedPassed -isnot [bool] -or -not $Receipt.automatedPassed -or
        $Receipt.completeSuite -isnot [bool] -or -not $Receipt.completeSuite) { throw 'Runtime receipt is not a complete passing suite.' }
    if ($Receipt.source.sha256 -cne $SourceHash) { throw 'Runtime receipt tested different source content.' }
    if ($Receipt.builtFromSource -isnot [bool] -or -not $Receipt.builtFromSource) { throw 'Runtime receipt reused unverified staged binaries.' }
    if (-not $ExpectedCases.Count -or @($ExpectedCases | Sort-Object -Unique).Count -ne $ExpectedCases.Count) { throw 'Expected runtime case inventory is empty or duplicated.' }
    if (@(Compare-Object @($ExpectedCases | Sort-Object) @($Receipt.selectedCases | Sort-Object)).Count -or
        $Receipt.runs.Count -ne $ExpectedCases.Count -or
        @($Receipt.runs.name | Sort-Object -Unique).Count -ne $ExpectedCases.Count -or
        @($Receipt.runs.result | Sort-Object -Unique).Count -ne $ExpectedCases.Count) { throw 'Runtime receipt has missing, duplicate, or unexpected cases/evidence.' }
    foreach ($run in $Receipt.runs) {
        if ($run.name -notin $ExpectedCases -or $run.outcome -ne 'passed' -or $run.exitCode -ne 0 -or
            -not $run.result -or -not (Test-Path -LiteralPath $run.result -PathType Leaf)) { throw "Runtime case lacks passing evidence: $($run.name)" }
        $evidence = Read-SmokeCaseResult $run.result $run.startedAt $run.finishedAt $run.exitCode
        if ($run.name -in @('features-normal','features-large')) {
            foreach ($feature in @('values','actions','launchers','providers')) {
                if ($evidence.settingsFeatures.$feature -isnot [bool] -or -not $evidence.settingsFeatures.$feature) {
                    throw "Runtime feature case omitted passing $feature checks: $($run.name)"
                }
            }
            if ($evidence.settingsFeatures.largeText -isnot [bool] -or $evidence.settingsFeatures.largeText -ne ($run.name -eq 'features-large')) {
                throw "Runtime feature case used the wrong movie variant: $($run.name)"
            }
        }
        if ((Get-FileHash -LiteralPath $run.result).Hash -cne $run.resultSha256) { throw "Runtime case has changed evidence: $($run.name)" }
    }
}

function Save-SmokeReport($Report, [string]$Directory) {
    $Report['featureResults'] = @(Get-SmokeFeatureResults $Report)
    [IO.File]::WriteAllText((Join-Path $Directory 'result.json'), ($Report | ConvertTo-Json -Depth 30))
    $lines = @('# OSF Settings smoke bench', '', "Outcome: **$($Report.outcome)**", '',
        "Scope: $($Report.scope)", "Complete automated suite: $($Report.completeAutomatedSuite)",
        '', '| Stage | Result | Seconds | Evidence / detail |', '| --- | --- | ---: | --- |')
    foreach ($stage in $Report.stages) {
        $link = if ($stage.log) { "[$($stage.name)](<$($stage.log)>) " } else { '' }
        $lines += "| $($stage.name) | $($stage.status) | $($stage.seconds) | $link$($stage.message.Replace('|','/').Replace("`n",' ')) |"
    }
    if ($Report.Contains('runtime') -and $Report.runtime) { $lines += @('', "Game suite receipt: [$($Report.runtime)](<$($Report.runtime)>)") }
    $lines += @('', '## Coverage', '', 'Cells report the stages containing the mapped checks. Native and game stages pass only when their entire suite passes; a failure does not identify which individual features failed. A passed layer does not establish the other layers.', '',
        '| Feature | Native suites | Preview variants | Game cases |', '| --- | --- | --- | --- |')
    foreach ($feature in $Report.coverage) {
        $result = @($Report.featureResults | Where-Object id -eq $feature.id)[0]
        $lines += "| $($feature.feature) | $($result.native): $($feature.native -join ', ') | $($result.preview): $($feature.preview -join ', ') | $($result.runtime): $($feature.runtime -join ', ') |"
    }
    $lines += @('', '## Not established by this run', '')
    foreach ($gate in $Report.unverified) { $lines += "- $gate" }
    [IO.File]::WriteAllLines((Join-Path $Directory 'report.md'), $lines)
    $settings = [Xml.XmlWriterSettings]::new(); $settings.Indent = $true
    $xml = [Xml.XmlWriter]::Create((Join-Path $Directory 'junit.xml'), $settings)
    try {
        $xml.WriteStartElement('testsuite'); $xml.WriteAttributeString('name', 'OSF Settings smoke bench')
        $xml.WriteAttributeString('tests', [string]$Report.stages.Count)
        $xml.WriteAttributeString('failures', [string]@($Report.stages | Where-Object status -eq 'failed').Count)
        $xml.WriteAttributeString('errors', [string]@($Report.stages | Where-Object status -eq 'blocked').Count)
        $xml.WriteAttributeString('skipped', [string]@($Report.stages | Where-Object status -in @('not-run','running')).Count)
        foreach ($stage in $Report.stages) {
            $xml.WriteStartElement('testcase'); $xml.WriteAttributeString('name', $stage.name)
            $xml.WriteAttributeString('time', ([double]$stage.seconds).ToString([Globalization.CultureInfo]::InvariantCulture))
            $element = switch ($stage.status) { failed { 'failure' } blocked { 'error' } not-run { 'skipped' } running { 'skipped' } default { '' } }
            if ($element) { $xml.WriteElementString($element, $stage.message) }
            if ($stage.log) { $xml.WriteElementString('system-out', $stage.log) }
            $xml.WriteEndElement()
        }
        $xml.WriteEndElement()
    } finally { $xml.Dispose() }
}
