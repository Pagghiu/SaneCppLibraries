# File Architecture

## Purpose

`File` owns low-level synchronous descriptor I/O for files, anonymous pipes, standard-handle duplicates, and named pipe endpoints. Future work in this library should keep the public model descriptor-centered: open or create an OS resource, expose it as `FileDescriptor` or `PipeDescriptor`, and let callers compose it with higher-level libraries.

## Architectural Shape

The library is a dependency-free platform abstraction over native file and pipe handles. Public types expose explicit options for blocking mode, inheritability, sync behavior, and exclusive creation. Descriptor metadata, permissions, synchronization, and truncation belong here when they are bound to an already-open descriptor. Public headers may expose stable Sane primitive types, but OS headers and platform-specific calls belong in implementation files.

Named pipe server/client APIs are part of this descriptor layer. Accepted or connected named pipe endpoints must become `PipeDescriptor` values so they can be read, written, closed, and integrated with Async through the same descriptor-shaped surface.

## Boundaries

`File` owns descriptor lifecycle and descriptor I/O. It does not own path-scoped filesystem operations such as recursive directory creation, path-based copy/delete/rename, or symlink metadata; those belong to `FileSystem`. It should not depend on Process, Async, AsyncStreams, Socket, Strings, Memory, or FileSystem. `sendfile` remains outside the public File API until there is a dependency-safe cross-library design.

## Similarities With Other Libraries

Like the other low-level platform libraries, `File` is dependency-free, allocation-conscious, and built on Common primitives such as `Result`, `Span`, `StringSpan`, `StringPath`, and `UniqueHandle`. It follows the same pattern of isolating OS handles behind portable public types while keeping OS headers out of public headers.

## Differences From Other Libraries

`File` is descriptor-first rather than path-first. Unlike `FileSystem`, it requires an opened descriptor for most operations and does not manage a base directory. Unlike Process or Async, it does not orchestrate subprocesses or event loops; it only exposes handles that those libraries may use. Unlike FileSystemWatcher, it reports direct I/O results rather than notifications.

## Inspirations

The evidenced inspiration is native OS descriptor and pipe APIs: POSIX file descriptors and pipe/socket endpoints, plus Windows file and named-pipe handles. The API intentionally resembles the capabilities of those native handles while using Sane C++ result/error and storage conventions.

## Anti-Inspirations

Inference: `File` should not become a `std::fstream`, `stdio`, or allocation-owning convenience layer. Inference: named pipes should not become a separate IPC framework with its own read/write abstraction. The project constraints also rule out exposing OS headers, STL types, exceptions, or hidden allocation in the public File surface.

## Architectural Choices

Keep descriptor ownership explicit and move-only. Keep path input small and native-aware through `StringSpan`/`StringPath`. Preserve `PipeDescriptor` as the common shape for anonymous pipes and connected named pipes. Add descriptor-bound capabilities here only when they do not require FileSystem-style path traversal or higher-level library dependencies. When an API would require cross-library coordination, prefer deferral over a convenient dependency.

## Explicitly Excluded Targets

Do not turn `File` into a recursive filesystem library, a subprocess library, an async runtime, a stream pipeline framework, or a portable replacement for every OS-specific file-control operation. Do not add public `sendfile` or zero-copy transfer APIs until their dependency and ownership boundaries are designed explicitly.

## Sources

- [File documentation](../../Documentation/Libraries/File.md)
- [File public API](../../Libraries/File/File.h)
- [File implementation](../../Libraries/File/File.cpp)
- [File tests](../../Tests/Libraries/File/FileTest.cpp)
- [FILE-0001 - Represent Named Pipes as File Pipe Descriptors](file-0001-represent-named-pipes-as-file-pipe-descriptors.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0007 - Keep public headers free of system and compiler headers](../Global/sc-0007-keep-public-headers-free-of-system-and-compiler-headers.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)

## Decision Log

- [FILE-0001 - Represent named pipes as File pipe descriptors](file-0001-represent-named-pipes-as-file-pipe-descriptors.md)
