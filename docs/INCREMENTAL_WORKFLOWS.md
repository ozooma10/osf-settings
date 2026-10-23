# Incremental smoke and release work

Normal and large previews keep separate prepared assets in `build/preview/normal`
and `build/preview/large`. The native row exporter remains in `build/preview`.
General and bindings checks keep separate logs, screenshots and successful receipts
under each size. All four test variants remain in both smoke and release plans.

Each preview request first performs the incremental asset build. It reuses test
evidence only when the actual movie, host, imported libraries, configuration,
Ruffle binary, runner and requested options match and all retained evidence hashes
are intact. Per-case Ruffle configuration is retained with the evidence. Reports record when those screenshots were tested, including reuse;
they never label reused screenshots as fresh. Missing or edited evidence reruns
the assertions. `test-menu-preview.ps1 -Force` bypasses reuse.

Smoke and release share workflow self-test receipts keyed on the project/harness
source identities (including submodules), PowerShell/OS and Git executable.
`-ForceVerification` on either entrypoint reruns those checks and all previews.
Native tests, package/archive validation, reinstall tests and runtime cases still
run normally. Game and archive results are never cached.

Harness Scaleform movies live in `build/scaleform/harness`, keeping production
movies intact. Acceptance Papyrus/ESM outputs are also reused by content hash.
Intimacy uses its own Settings native output directory, `build/intimacy-test-host`.

`pwsh -NoProfile -File tests/preview_workflow_tests.ps1` exercises the real preview
runner with a simulated process: all four variants, warm reuse, original dates,
changed movies, missing screenshots, failure and retry. It launches no browser.
