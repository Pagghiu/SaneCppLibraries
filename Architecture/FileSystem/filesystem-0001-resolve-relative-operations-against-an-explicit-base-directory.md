# FILESYSTEM-0001 - Resolve Relative Operations Against an Explicit Base Directory

Status: Accepted
Date: 2026-07-04

## Context

Filesystem operations are often written with relative paths, but resolving them against the process current working directory makes behavior depend on ambient mutable process state. That is hard to reason about in tests, tools, plugins, and applications that may change the process directory for unrelated reasons.

## Decision

`FileSystem::init` and `FileSystem::changeDirectory` establish the explicit base directory for a `FileSystem` instance. Operations may accept absolute or relative paths, but relative paths are resolved against that instance base directory, not against the process current working directory.

## Consequences

Callers must initialize a `FileSystem` before using relative paths, and changing the process current directory does not silently redirect operations. Tests and tools can create scoped filesystem contexts without relying on global process state. Absolute paths remain available when a call should bypass the instance base directory.

## Confirmation

A change preserves this decision when relative path operations continue to use the `FileSystem` instance directory, documentation states that relative paths are not process-CWD relative, and tests exercise `init`, `changeDirectory`, and relative file/directory operations.

## Related

- [FileSystem documentation](../../Documentation/Libraries/FileSystem.md)
- [FileSystem public API](../../Libraries/FileSystem/FileSystem.h)
- [FileSystem implementation](../../Libraries/FileSystem/FileSystem.cpp)
- [FileSystem tests](../../Tests/Libraries/FileSystem/FileSystemTest.cpp)
