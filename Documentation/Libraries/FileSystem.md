@page library_file_system File System

@brief 🟩 Synchronous, path-based file and directory operations

[TOC]

[SaneCppFileSystem.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystem.h)
provides portable operations on filesystem paths: create, copy, rename and remove entries; read or replace complete files;
inspect metadata; and work with links and permissions.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](FileSystem.svg)


# When to use File System

Use File System when the unit of work is a **path or directory tree**, rather than an already-open byte stream. It fits
setup and cleanup code, installers, build tools, caches, configuration loading and other synchronous workflows that need
the same behavior on Windows, macOS and Linux.

The library covers the common path-level operations:

- creating one directory or an entire missing hierarchy;
- copying, renaming and removing files or directory trees;
- replacing, appending or reading a complete file;
- checking entry type and access, or retrieving richer metadata;
- creating and inspecting symbolic links and hard links; and
- querying executable, application-root and process working-directory paths.

It is not a directory enumerator, an open-file abstraction or an asynchronous filesystem. Use
[File System Iterator](@ref library_file_system_iterator) to discover directory contents, [File](@ref library_file) for
incremental reads, writes, seeking, durability and descriptor ownership, and [Async](@ref library_async) when blocking the
calling thread is unacceptable.

# The mental model: a private path base

An SC::FileSystem object holds a base directory. Call SC::FileSystem::init with an absolute directory, then every relative
path passed to that object is resolved against this base. Absolute paths remain absolute. Importantly, relative operations
do **not** depend on the process-wide current working directory, so a library or another thread changing that global state
does not silently redirect them. SC::FileSystem::changeDirectory changes only this object's base.

This source-backed example is compiled and run as part of `FileSystemTest`:

\snippet Tests/Libraries/FileSystem/FileSystemTest.cpp FileSystemQuickSheetSnippet

The example also shows the deliberately conservative copy default: an existing destination is an error. Set
SC::FileSystemCopyFlags::overwrite explicitly when replacement is intended. Filesystem cloning is requested by default
where the platform supports it; disabling that optimization asks for a regular copy without changing the result's logical
contents.

Several plural operations accept caller-provided spans, such as SC::FileSystem::copyFiles and
SC::FileSystem::removeFiles. They are convenience batches, not transactions: processing stops at the first failure and
earlier successful operations are not rolled back.

# Storage, lifetime and failure behavior

SC::FileSystem owns fixed-capacity path-conversion and error buffers. Ordinary path operations do not allocate internally,
and SC::StringSpan inputs are borrowed only for the duration of the call. The practical consequence is that paths must fit
SC::StringPath capacity; overlong or unrepresentable paths fail through SC::Result rather than growing hidden storage.

Whole-file writes borrow the supplied SC::Span or SC::StringSpan. Whole-file reads are different: the caller supplies a
`String`, `Buffer`, `SmallString` or another growable-buffer-compatible destination, and that destination decides whether
growth allocates or fails at fixed capacity. For large files, bounded memory, seeking or partial I/O, use
[File](@ref library_file) instead.

Mutating and metadata operations return SC::Result and can fail for normal filesystem reasons: missing parents, access
rules, occupied destinations, unsupported operations or I/O errors. Boolean queries such as SC::FileSystem::exists and
SC::FileSystem::canAccess intentionally collapse "no" and lookup failure into `false`; use a Result-returning operation
when the distinction matters. Setting SC::FileSystem::preciseErrorMessages asks the object to format platform error detail
into its internal fixed buffer; it does not change success semantics.

The API is synchronous. A recursive copy or removal may take an unbounded amount of wall-clock time even though its memory
use is controlled, so it should not run on a latency-sensitive event-loop thread.

# Paths, encodings and the low-level escape hatch

The object API performs path normalization and encoding conversion. On Windows it accepts UTF-8 or UTF-16 string spans and
uses UTF-16 for native calls; on POSIX paths are UTF-8/native byte strings. Returned filesystem paths use the operating
system's native encoding. [Strings](@ref library_strings), especially SC::Path, is the companion library for parsing and
composing paths before passing them here.

SC::FileSystem::Operations exposes the underlying stateless operations when paths are already in native encoding: UTF-16
on Windows and UTF-8 on POSIX. It also provides SC::FileSystem::Operations::getExecutablePath,
SC::FileSystem::Operations::getApplicationRootDirectory and
SC::FileSystem::Operations::getCurrentWorkingDirectory, all writing into a caller-owned SC::StringPath. Prefer the object
API for application paths; `Operations` deliberately gives up the object's stable base and conversion layer.

# Metadata and links require deliberate semantics

SC::FileSystem::stat follows a symbolic link, while SC::FileSystem::lstat describes the link itself. Both fill
SC::FileSystemStat with portable entry type, size, link count and timestamps, plus platform-specific POSIX identity/mode
fields or Windows attributes and file identity. SC::FileSystem::getFileStat is retained as a legacy alias for `stat`.

SC::FileSystem::existsAndIsLink and SC::FileSystem::readSymbolicLink operate on the link rather than its target.
SC::FileSystem::removeLinkIfExists is the corresponding explicit cleanup operation. Ownership and permission calls expose
native numeric concepts: `chmod` and `chown` follow links, while `lchmod` and `lchown` target the link where the operating
system supports that distinction. These APIs are portable entry points, not a promise that Windows and POSIX permission
models are equivalent.

Recursive directory operations deserve the same caution. SC::FileSystem::removeDirectoryRecursive is the equivalent of
an `rm -rf`-style destructive operation, and SC::FileSystem::copyDirectory recursively materializes a tree. Neither is an
atomic snapshot or recovery mechanism. Applications that replace durable state should build their own staging, sync and
rename protocol using File and File System primitives.

# Boundaries and tradeoffs

File System favors explicit buffers, synchronous results and a small portable surface over a `std::filesystem`-like object
graph. It does not allocate path objects for every entry, throw exceptions, preserve a process-global working-directory
assumption, traverse directories, watch for changes or provide filesystem transactions.

That makes it a good fit for controlled system operations where the caller owns memory and error policy. It is a poor fit
for streaming multi-gigabyte files, interactive event loops, directory search or change notification. The neighboring
libraries divide those responsibilities cleanly:

- [File](@ref library_file) owns open descriptors and performs incremental byte I/O;
- [File System Iterator](@ref library_file_system_iterator) enumerates directory entries;
- [File System Watcher](@ref library_file_system_watcher) reports later changes; and
- [Async](@ref library_async) integrates supported I/O with an event loop.

For the complete API surface and option fields, see the [FileSystem module](@ref group_file_system).

# Status
🟩 Usable
The library covers common path operations, links, access checks and rich path metadata. Filesystem-level queries such as
`statfs` are not yet implemented. Permission, ownership, link and clone behavior necessarily retains platform-specific
limits, and callers should handle an unsupported or access-denied Result.

# Blog

Relevant development updates:

- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)
- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)

# Roadmap

🟦 Complete Features:
- `statfs`

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/FileSystem`.
Single File counts
`SaneCppFileSystem.h`.
Standalone counts `SaneCppFileSystemStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 188		| 1858		| 2046	|
| Single File | 1095		| 2308		| 3403	|
| Standalone  | 1095		| 2308		| 3403	|
