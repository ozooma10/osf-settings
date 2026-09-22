# Release packaging

Run from a Windows checkout with its submodules initialized, XMake 3.0.0+,
MSVC with C++23 support, Python 3, PowerShell 7.2+, and the Scaleform tools installed by
`tools/setup-scaleform.ps1`. The Papyrus helper also needs the Creation Kit
compiler and vanilla imports; see `tools/build-papyrus.ps1` for its defaults.

```powershell
pwsh -NoProfile -File packaging/build-archive.ps1
pwsh -NoProfile -File packaging/build-archive.ps1 -Label rc2
```

The version comes from `xmake.lua`; the label defaults to `rc1`. Each invocation
creates a new directory under `build/packages`, with a ZIP, `.zip.sha256`, and
`.manifest.json`. Existing output is never replaced. The manifest records the
source revision, dirty status, submodule revisions, configuration, and the size
and SHA-256 of every payload file. A dirty checkout is permitted and recorded;
freeze the intended source before publishing.

The script pins XMake to this repository, configures Windows x64 `releasedbg`
with `test_harness=n`, and rebuilds the production target sequentially. The build
also recompiles the public PEX and both SWFs. It temporarily points installation
at fresh staging, restores the caller's deployment environment even on failure,
and leaves XMake in production configuration. Normal subsequent build/install
commands use the caller's usual deployment path.

## Payload

The ZIP root maps directly to `Data`:

```text
SFSE/Plugins/OSFSettings.dll
SFSE/Plugins/OSF/Settings/schemas/osfsettings.json
SFSE/Plugins/OSF/Settings/translations/en/osfsettings.json
Interface/OSFSettingsMenu.swf
Interface/OSFSettingsMenu_LRG.swf
Scripts/OSFSettings.pex
Scripts/Source/OSFSettings.psc
Docs/OSFSettings/README.txt
Docs/OSFSettings/LICENSE
Docs/OSFSettings/EXCEPTIONS
Docs/OSFSettings/THIRD_PARTY_NOTICES.txt
Docs/OSFSettings/CommonLibSF-COPYING
Docs/OSFSettings/CommonLibSF-EXCEPTIONS
Docs/OSFSettings/CommonLibShared-LICENSE
Docs/OSFSettings/CommonLibShared-EXCEPTIONS
```

An explicit allowlist excludes examples, the learning schema, acceptance ESM
and scripts, harness configuration, personal values, native Controls and PDBs.
Unexpected staged files cause failure (the build's PDB is allowed in staging
only). The script rejects known test exports and movie observation/preview
markers, then reopens the ZIP and verifies every entry's name, size and hash.
The final ZIP name is assigned only after verification succeeds.

Archive entry order and timestamps are fixed for a given payload. This is a
repeatable build/package procedure, not a claim that independent native or
Papyrus compilations produce identical bytes. Staging and PDBs are retained
locally beside the archive; upload only the ZIP and desired checksum/manifest.

## Release check

The instrumented [runtime acceptance](RELEASE_ACCEPTANCE.md) is separate from
verification of the final production archive. Before publishing:

1. Install the actual ZIP in a clean mod-manager profile with its requirements.
2. Verify Pause/F10 entry, both menu sizes and a real supported consumer.
3. Change a value and binding, restart, and verify persistence.
4. Replace/reinstall the framework package and verify those values survive.
5. Confirm no Test Mod or acceptance fixture appears, and collect screenshots
   for the release page using supported mods.

This packaging command does not launch Starfield, change profiles, tag, commit,
push or publish. Production installation does not delete files from existing
development deployments: an old `learning.json` may remain there. Use a clean
package installation for release checks rather than deleting shared user data.
