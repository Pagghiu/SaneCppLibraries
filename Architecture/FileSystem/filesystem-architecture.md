# FileSystem Architecture

## Purpose

`FileSystem` owns path-based filesystem manipulation: create, copy, remove, rename, link, query, read, write, and path metadata operations. Future work should keep it focused on operations where the path is the primary object, not an already-open descriptor.

## Architectural Shape

The library is a dependency-free path-operation facade over native OS filesystem APIs. A `FileSystem` instance has an explicit base directory established by `init` or `changeDirectory`; relative paths are resolved against that instance state. Lower-level `FileSystem::Operations` functions provide native-encoded path operations when callers or implementation code already have fully resolved paths.

File-like path operations stay in this library when they are path-scoped convenience operations. They should use Common primitives and native calls directly instead of depending on `File`, `Strings`, `Memory`, or `Time`.

## Boundaries

`FileSystem` owns path-scoped operations and base-directory behavior. It does not own descriptor positioning, descriptor-bound permissions, descriptor synchronization, pipes, named pipes, or process I/O; those belong to `File` or higher libraries. It should not become a general path parser; callers that need path parsing/composition should use Strings path utilities outside the library boundary.

## Similarities With Other Libraries

Like File, FileSystemIterator, and FileSystemWatcher, `FileSystem` uses native OS APIs behind a portable Sane C++ API and keeps its dependency graph empty. It shares the Common path and result primitives that let these libraries stay standalone and single-file friendly.

## Differences From Other Libraries

Unlike `File`, `FileSystem` is path-first and may open descriptors only as an implementation detail. Unlike `FileSystemIterator`, it performs concrete mutations and reads/writes rather than enumerating entries. Unlike `FileSystemWatcher`, it performs immediate operations rather than delivering asynchronous change notifications.

## Inspirations

The evidenced inspirations are native filesystem operations such as POSIX path APIs, Windows file attributes/reparse point APIs, and platform copy/clone facilities. The API also follows common shell/file-manager operations such as copy, remove, mkdir, rename, chmod, chown, stat, and symlink handling.

## Anti-Inspirations

Inference: `FileSystem` should not be `std::filesystem` with Sane naming; it deliberately avoids STL types and dependency-heavy path ownership. Inference: it should not route simple path operations through `File` merely to reduce implementation code. It should not make behavior depend on the process current working directory unless the caller explicitly chooses that directory as the instance base.

## Architectural Choices

Resolve relative paths through the instance base directory. Keep dependency metadata empty even when that means duplicating native file-like implementation code. Use `StringPath` and native transport buffers for fixed path storage. Preserve the split between the stateful `FileSystem` facade and stateless low-level `Operations`. Treat filesystem-level queries such as `statfs` as future work rather than stretching existing metadata APIs prematurely.

## Explicitly Excluded Targets

Do not add hidden allocation, recursive traversal callbacks, descriptor seek/read/write positioning APIs, file watching, process current-directory coupling, or mandatory dependencies on File, Strings, Memory, or Time. Do not make path parsing/composition a core responsibility of this library.

## Sources

- [FileSystem documentation](../../Documentation/Libraries/FileSystem.md)
- [FileSystem public API](../../Libraries/FileSystem/FileSystem.h)
- [FileSystem implementation](../../Libraries/FileSystem/FileSystem.cpp)
- [FileSystem tests](../../Tests/Libraries/FileSystem/FileSystemTest.cpp)
- [Dependency metadata](../../Support/Dependencies/Dependencies.json)
- [FILESYSTEM-0001 - Resolve Relative Operations Against an Explicit Base Directory](filesystem-0001-resolve-relative-operations-against-an-explicit-base-directory.md)
- [FILESYSTEM-0002 - Keep FileSystem Dependency-Free Even for File-Like Operations](filesystem-0002-keep-filesystem-dependency-free-even-for-file-like-operations.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [COMMON-0008 - Keep StringSpan and StringPath in Common](../Common/common-0008-keep-stringspan-and-stringpath-in-common.md)

## Decision Log

- [FILESYSTEM-0001 - Resolve relative operations against an explicit base directory](filesystem-0001-resolve-relative-operations-against-an-explicit-base-directory.md)
- [FILESYSTEM-0002 - Keep FileSystem dependency-free even for file-like operations](filesystem-0002-keep-filesystem-dependency-free-even-for-file-like-operations.md)
