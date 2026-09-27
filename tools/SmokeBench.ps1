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

function Test-SmokeRuntimeReceipt($Receipt, [string[]]$ExpectedCases, [string]$SourceHash) {
    if ($Receipt.automatedPassed -isnot [bool] -or -not $Receipt.automatedPassed -or
        $Receipt.completeSuite -isnot [bool] -or -not $Receipt.completeSuite) { throw 'Runtime receipt is not a complete passing suite.' }
    if ($Receipt.source.sha256 -cne $SourceHash) { throw 'Runtime receipt tested different source content.' }
    if (@(Compare-Object @($ExpectedCases | Sort-Object) @($Receipt.selectedCases | Sort-Object)).Count -or
        $Receipt.runs.Count -ne $ExpectedCases.Count -or
        @($Receipt.runs.name | Sort-Object -Unique).Count -ne $ExpectedCases.Count) { throw 'Runtime receipt has missing, duplicate, or unexpected cases.' }
    foreach ($run in $Receipt.runs) {
        if ($run.name -notin $ExpectedCases -or $run.outcome -ne 'passed' -or $run.exitCode -ne 0 -or
            -not $run.result -or -not (Test-Path -LiteralPath $run.result -PathType Leaf)) { throw "Runtime case lacks passing evidence: $($run.name)" }
        $result = Get-Content -LiteralPath $run.result -Raw | ConvertFrom-Json
        if ($result.outcome -ne 'passed' -or $result.scenario -ne 'SettingsSmoke' -or -not $result.finishedAt -or
            (Get-FileHash -LiteralPath $run.result).Hash -cne $run.resultSha256) { throw "Runtime case has changed or invalid evidence: $($run.name)" }
    }
}

function Save-SmokeReport($Report, [string]$Directory) {
    [IO.File]::WriteAllText((Join-Path $Directory 'result.json'), ($Report | ConvertTo-Json -Depth 30))
    $lines = @('# OSF Settings smoke bench', '', "Outcome: **$($Report.outcome)**", '',
        "Scope: $($Report.scope)", "Complete automated suite: $($Report.completeAutomatedSuite)",
        '', '| Stage | Result | Seconds | Evidence / detail |', '| --- | --- | ---: | --- |')
    foreach ($stage in $Report.stages) {
        $link = if ($stage.log) { "[$($stage.name)](<$($stage.log)>) " } else { '' }
        $lines += "| $($stage.name) | $($stage.status) | $($stage.seconds) | $link$($stage.message.Replace('|','/').Replace("`n",' ')) |"
    }
    if ($Report.Contains('runtime') -and $Report.runtime) { $lines += @('', "Game suite receipt: [$($Report.runtime)](<$($Report.runtime)>)") }
    $lines += @('', '## Coverage', '', '| Feature | Native suites | Game cases |', '| --- | --- | --- |')
    foreach ($feature in $Report.coverage) { $lines += "| $($feature.feature) | $($feature.native -join ', ') | $($feature.runtime -join ', ') |" }
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
