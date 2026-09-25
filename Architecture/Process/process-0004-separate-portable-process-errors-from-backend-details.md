# PROCESS-0004 - Separate Portable Process Errors From Backend Details

Status: Accepted
Date: 2026-09-13

## Context

Process launch and waiting use different native stages on supported platforms. POSIX launch can fail while executing
the child, whereas Windows launch can fail in `CreateProcess`. Exposing those backend stages as the primary
`ProcessError` made the same public operation report different identities across platforms and forced callers to
branch on implementation details.

The native error number is useful for diagnosis, but it cannot identify which lower-level stage produced it. Plain
`Result` must remain a portable category/error value and cannot carry Process-specific diagnostic state.

## Decision

`ProcessError` contains only stable platform-agnostic failures visible at the Process API boundary. In particular, a
failed executable launch always reports `ProcessError::LaunchFailed`, whether the backend failed in POSIX execution
or Windows process creation.

`ProcessFork::fork` reports `ProcessError::CloneFailed`, without exposing the native operation as its primary
identity. POSIX `fork` and Windows `RtlCloneUserProcess` remain distinguishable through `ProcessErrorDetail`.
Likewise, a POSIX-only failure while building an executable path during launch reports `LaunchFailed` with
`PosixBuildResolveExecutablePath` detail instead of creating an internal buffer-construction primary error.

Argument and environment capacity failures remain portable primary errors: `ArgumentCapacityExceeded` and
`EnvironmentCapacityExceeded`.
The string arena is shared by argument and environment construction and is also reused by Plugin. Its caller supplies
a structured capacity Result owned by the calling library, so exhaustion of its entry table or destination view never
falls back to an embedded implementation-specific literal or silently takes Process ownership for Plugin failures.

`ResultProcess` retains the authoritative plain `Result` plus a public stable `ProcessErrorDetail` and native numeric
error value. Details identify lower-level backend stages with explicit platform names where relevant, such as
`PosixExec` and `WindowsCreateProcess`. They are diagnostic and programmatically inspectable, but are not encoded
when the enriched result is converted to plain `Result`.

`ResultProcess` remains allocation-free, non-owning, trivially copyable, and standard-layout. During the temporary
legacy `Result` bridge it is 24 bytes on 64-bit builds; when the legacy message pointer is removed, the plain result
is eight bytes and `ResultProcess` reaches the 16-byte enriched-result target. Shared generic helpers continue to
return their own plain `Result` failures through the existing conversion boundary, with Process detail cleared rather
than remapping a foreign error to a Process code.

The optional formatter reports the portable primary message and appends canonical English backend-stage wording plus
the native numeric value when present; it never exposes raw enum identifiers as diagnostic text.

## Consequences

Applications can use one primary error identity for the same Process operation on every supported platform, then
inspect the detail only when backend-specific diagnostics or recovery are useful. Existing plain `Result` consumers
remain deliberately unaware of Process detail. The public detail enum is append-only, so new backend stages require
new values rather than reinterpretation of existing ones.

## Confirmation

Process tests assert that a missing executable reports `LaunchFailed` on both POSIX and Windows, with respectively
`PosixExec` and `WindowsCreateProcess` detail. They also assert layout traits and both the bridge and final-size
contracts. Focused Debug and Release Process tests, result-category validation, and single-file compilation verify
the migration boundaries.

## Related

- [Process public API](../../Libraries/Process/Process.h)
- [Process error formatter](../../Libraries/Process/ProcessErrorFormatter.h)
- [Process tests](../../Tests/Libraries/Process/ProcessTest.cpp)
- [COMMON-0009 - Use library-owned structured Result errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [SC-0006 - Use explicit Result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
