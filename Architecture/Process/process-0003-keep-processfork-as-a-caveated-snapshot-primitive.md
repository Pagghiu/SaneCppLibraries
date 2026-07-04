# PROCESS-0003 - Keep ProcessFork as a Caveated Snapshot Primitive

Status: Accepted
Date: 2026-07-04

## Context

Fork-style process cloning is useful for snapshotting current process memory without copying all data up front, but the semantics are platform-specific and surprising. POSIX `fork` and Windows process cloning have sharp limits around threads, inherited handles, GUI state, debugger behavior, and emulated Windows processes.

## Decision

`ProcessFork` remains a narrow snapshot primitive. It exposes parent/child side detection, suspended or immediate start, pipe-based coordination, and explicit child waiting. It is not a general task runner, plugin sandbox, or safe replacement for ordinary child process launching.

## Consequences

The feature can support copy-on-write snapshot workflows, such as writing a consistent view of memory, without broadening Process into a sandboxing framework. Callers must keep forked-child behavior simple and must account for platform caveats. Tests may skip unsupported Windows emulation cases.

## Confirmation

A change preserves this decision when `ProcessFork` documentation and tests continue to present it as a constrained snapshot tool, child-side examples avoid broad application APIs, handle inheritance requirements stay explicit, and unsupported platform/emulation cases are detected or skipped rather than papered over.

## Related

- [Process documentation](../../Documentation/Libraries/Process.md)
- [ProcessFork public API](../../Libraries/Process/Process.h)
- [ProcessFork tests](../../Tests/Libraries/Process/ProcessTest.cpp)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
