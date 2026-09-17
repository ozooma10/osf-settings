# Mod Issues native API

Include [OSFSettings_Diagnostics.h](../sdk/OSFSettings_Diagnostics.h) with
`OSFSettings.h` alongside it. The header uses CommonLibSF, like the settings SDK.

```cpp
OSFSettings::API::Diagnostics::Client diagnostics;

// During SFSE kPostPostLoad:
if (diagnostics.Init()) {
    const auto status = diagnostics.Report({
        .modId = "mymod",
        .id = "missing-animation-pack",
        .title = "Custom animations are unavailable"
    });
    // Check status before treating the report as accepted.
}

// When the mod confirms recovery:
diagnostics.Clear("mymod", "missing-animation-pack");
```

## Contract

- `modId` identifies the reporting mod. No registration or settings schema is
  required. Display names use the current schema title when available, otherwise
  the mod ID. IDs follow the settings rules: lowercase ASCII letters, digits,
  dots, underscores and hyphens; empty IDs, `.` and `..` are invalid.
- `id` is a stable, case-sensitive issue ID within that mod. Reporting the same
  `(modId, id)` replaces its contents without adding another entry.
- Issue ID and title must contain non-whitespace text. `impact` and `nextSteps`
  are optional: omit them or pass null/empty strings. Each report replaces the
  whole issue, so omitting a field clears its previous text. All supplied strings
  are NUL-terminated UTF-8; the first NUL ends the value.
- `Warning` describes degraded behavior or a meaningful limitation. `Error`
  describes a failed operation or unavailable feature. Severity defaults to `Warning`.
- `Clear(modId, id)` removes one issue. `ClearMod(modId)` removes all issues
  belonging to that mod. Both succeed when the requested issues are already absent.
- Issues exist only for the current game process. Clearing removes them entirely.
  The reporting mod must reassess conditions after relevant configuration or
  save changes, and clear issues only when they no longer apply. Opening Settings
  does not run checks or clear reports.

## Ownership and threading

Report arguments remain caller-owned and must stay valid and unchanged until
the call returns. OSF Settings copies them into its own storage; callers can then
reuse or release their buffers. No STL-owned objects cross the interface.

`Report`, `Clear` and `ClearMod` may run on any native thread. Calls complete
synchronously and access the same synchronized registry. Simultaneous calls are
serialized; callers must coordinate ordering when it matters. These operations
perform no engine work, invoke no callbacks, and need no settings initialization.

Initialize or attach a `Client` before sharing it across threads. Do not race
`Init` or `Attach` against other calls on that client. Acquired interfaces belong
to OSF Settings for the process lifetime; do not delete them.

## Results and versions

Methods return `OSFSettings::API::Status`:

| Status | Meaning |
| --- | --- |
| `Ok` | Report stored/updated, or the requested issues are absent after clearing. |
| `NotReady` | The SDK client has no attached service. |
| `InvalidArgument` | A required pointer is null or required text/identity is invalid. Existing issues are unchanged. |
| `InternalError` | An internal operation failed, such as allocating report storage. |

The diagnostics ABI starts at `1.0` (`0x00010000`) and is independent of the
settings ABI and plugin release version. `RequestInterface()` uses the already
loaded `OSFSettings.dll` and its `OSFSettings_RequestDiagnosticsAPI` export; it
does not load the plugin. Unsupported versions return null and set the optional
output version to zero. On success the output contains the provider's version.
After launch, existing vtable slots and `Issue` layout are fixed for this ABI.

A [development-only native example](../examples/diagnostics/README.md) demonstrates
reporting, updating and clearing without a settings schema. Menu presentation
and Papyrus support remain separate work.
