# FILE-0001 - Represent Named Pipes as File Pipe Descriptors

Status: Accepted
Date: 2026-07-04

## Context

Named pipe and local IPC facilities differ by platform: Windows uses named pipe handles, while POSIX implementations use filesystem-named socket endpoints. The rest of the project already composes around `FileDescriptor` and `PipeDescriptor` for synchronous I/O, process redirection, and optional Async integration. Adding a separate named-pipe abstraction disconnected from those descriptor types would duplicate I/O APIs and make Process or Async dependencies tempting.

## Decision

`File` represents accepted or connected named pipe endpoints as `PipeDescriptor` values. `NamedPipeServer` and `NamedPipeClient` own endpoint creation and connection, while `NamedPipeName::build` converts a logical name into a platform-native endpoint name. Higher-level libraries may compose with the returned descriptors without `File` depending on Process, Async, or AsyncStreams.

## Consequences

Named pipes remain part of the low-level descriptor library instead of becoming a separate IPC framework. Callers get one read/write surface for anonymous pipes, named pipes, and descriptor-based Async composition. The abstraction intentionally exposes platform-native endpoint names at the create/connect boundary, with a helper for common logical-name construction.

## Confirmation

A change preserves this decision when named pipe server/client APIs still return connected `PipeDescriptor` endpoints, named pipe functionality remains in `File`, tests cover create/connect/accept behavior on supported platforms, and dependency metadata does not add Process, Async, AsyncStreams, or Socket as `File` dependencies.

## Related

- [File documentation](../../Documentation/Libraries/File.md)
- [File public API](../../Libraries/File/File.h)
- [File implementation](../../Libraries/File/File.cpp)
- [File tests](../../Tests/Libraries/File/FileTest.cpp)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
