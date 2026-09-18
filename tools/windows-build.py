#!/usr/bin/env python3
"""Snapshot Mac source, transfer over SSH, compile on Windows, and fetch evidence."""

import argparse
import base64
import json
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
WINDOWS_BUILDS = "C:/Modding/Starfield/OSF Settings Mac Builds"


def ps_string(value):
    return "'" + value.replace("'", "''") + "'"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="osf-windows", help="An authenticated Windows OpenSSH host alias")
    parser.add_argument("--ssh-config", type=Path, default=Path.home() / "Library/Application Support/OSF Windows Bridge/ssh_config")
    options = parser.parse_args()
    if options.host.startswith("-"):
        parser.error("Invalid SSH host alias")
    if not options.ssh_config.is_file():
        parser.error(f"SSH bridge has not been configured: {options.ssh_config}")
    common = ["-F", str(options.ssh_config), "-o", "BatchMode=yes", "-o", "ConnectTimeout=15"]

    def remote(script, capture=False):
        payload = base64.b64encode(script.encode("utf-16le")).decode()
        return subprocess.run(
            ["ssh", *common, options.host, f"pwsh -NoProfile -NonInteractive -EncodedCommand {payload}"],
            text=True, capture_output=capture,
        )

    # Prove reachability before taking a snapshot or starting a Windows mutation.
    probe = remote("[Environment]::MachineName; $PSVersionTable.PSVersion.ToString()", capture=True)
    if probe.returncode:
        sys.stderr.write(probe.stderr)
        return probe.returncode
    print("Windows preflight:\n" + probe.stdout, flush=True)
    packaged = subprocess.check_output([sys.executable, str(ROOT / "tools/snapshot-source.py")], text=True)
    metadata = json.loads(packaged)
    archive = Path(metadata["archive"])
    snapshot_id = metadata["snapshot_id"]
    local_results = archive.parent / f"{snapshot_id}-results"
    local_results.mkdir(exist_ok=False)
    (local_results / "transfer.json").write_text(json.dumps(metadata, indent=2) + "\n")
    windows_archive = WINDOWS_BUILDS + "/incoming/" + archive.name
    windows_snapshot = WINDOWS_BUILDS + "/" + snapshot_id
    preparation = remote(f"""
$ErrorActionPreference = 'Stop'
$builds = {ps_string(WINDOWS_BUILDS)}
$archive = {ps_string(windows_archive)}
$snapshot = {ps_string(windows_snapshot)}
if ((Test-Path -LiteralPath $archive) -or (Test-Path -LiteralPath $snapshot)) {{ throw 'Snapshot destination already exists' }}
New-Item -ItemType Directory -Force -Path (Join-Path $builds 'incoming') | Out-Null
$ancestor = Get-Item -LiteralPath (Join-Path $builds 'incoming')
while ($null -ne $ancestor) {{
    if ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) {{ throw 'Build destination has a reparse point' }}
    $ancestor = $ancestor.Parent
}}
""")
    if preparation.returncode:
        return preparation.returncode
    subprocess.run(["scp", *common, str(archive), f"{options.host}:{windows_archive}"], check=True)
    build_script = f"""
$ErrorActionPreference = 'Stop'
$archive = {ps_string(windows_archive)}
$snapshot = {ps_string(windows_snapshot)}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ine {ps_string(metadata['archive_sha256'])}) {{ throw 'Transferred archive checksum mismatch' }}
if (Test-Path -LiteralPath $snapshot) {{ throw 'Snapshot destination already exists' }}
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {{
    $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $zip.Entries) {{
        $name = $entry.FullName
        if (-not $name.StartsWith({ps_string(snapshot_id + '/')}, [StringComparison]::Ordinal) -or $name -match '[\\\\:\\x00-\\x1f]' -or $name -match '(^|/)(\\.|\\.\\.)(/|$)' -or -not $names.Add($name)) {{ throw "Unsafe archive entry: $name" }}
    }}
}} finally {{ $zip.Dispose() }}
Expand-Archive -LiteralPath $archive -DestinationPath {ps_string(WINDOWS_BUILDS)}
$env:GIT_OPTIONAL_LOCKS = '0'
$development = 'C:/Modding/Starfield/OSF Settings Slim'
$before = @(& git -C $development status --porcelain=v1 --untracked-files=all)
if ($LASTEXITCODE -ne 0) {{ throw 'Could not inspect Windows development state' }}
$before | Set-Content -LiteralPath (Join-Path $snapshot 'development-status-before.txt') -Encoding utf8
& pwsh -NoProfile -File (Join-Path $snapshot 'source/tools/build-windows-snapshot.ps1') -SnapshotRoot $snapshot
$buildCode = $LASTEXITCODE
$after = @(& git -C $development status --porcelain=v1 --untracked-files=all)
if ($LASTEXITCODE -ne 0) {{ throw 'Could not inspect Windows development state after build' }}
$after | Set-Content -LiteralPath (Join-Path $snapshot 'development-status-after.txt') -Encoding utf8
if (($before -join "`n") -cne ($after -join "`n")) {{ Write-Warning 'Windows development Git status changed during this run; inspect concurrent work' }}
exit $buildCode
"""
    (local_results / "remote-request.ps1").write_text(build_script)
    execution = remote(build_script, capture=True)
    (local_results / "transport-stdout.log").write_text(execution.stdout)
    (local_results / "transport-stderr.log").write_text(execution.stderr)
    print(execution.stdout, end="")
    sys.stderr.write(execution.stderr)
    # Fetch evidence even when compilation fails. Never retry by changing source.
    fetched = subprocess.run(["scp", *common, "-r", f"{options.host}:{windows_snapshot}/reports", str(local_results)])
    for name in ("development-status-before.txt", "development-status-after.txt"):
        subprocess.run(["scp", *common, f"{options.host}:{windows_snapshot}/{name}", str(local_results)])
    result_path = local_results / "reports/result.json"
    if fetched.returncode or not result_path.is_file():
        print(f"No complete compiler report was returned. Transport evidence: {local_results}", file=sys.stderr)
        return execution.returncode or 1
    result = json.loads(result_path.read_text(encoding="utf-8-sig"))
    if result.get("source_id") != metadata["source_id"] or result.get("manifest_sha256") != metadata["manifest_sha256"]:
        raise RuntimeError("Returned compiler report identifies a different source snapshot")
    print(f"\n{result['status']}: {snapshot_id}\nSource SHA-256: {metadata['source_id']}\nReport: {result_path}")
    return execution.returncode or result.get("exit_code", 1)


if __name__ == "__main__":
    sys.exit(main())
