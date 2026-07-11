@page library_file_system_iterator File System Iterator

@brief 🟩 Allocation-free, pull-based directory traversal

[TOC]

`SC::FileSystemIterator` enumerates the immediate contents of a directory or walks a directory tree. It is a good fit when
code should process entries as they arrive, keep memory use bounded, and avoid both STL iterators and a callback-driven
API. It deliberately does not build a list of names, sort results, or return rich file metadata.

[SaneCppFileSystemIterator.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystemIterator.h)
is the amalgamated single-header distribution.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](FileSystemIterator.svg)


# The traversal model

The iterator owns the current path and the operating-system enumeration handles, while the caller supplies a span of
SC::FileSystemIterator::FolderState objects for the traversal stack. After SC::FileSystemIterator::init, each successful
SC::FileSystemIterator::enumerateNext call exposes one SC::FileSystemIterator::Entry through
SC::FileSystemIterator::get. Traversal is depth-first when recursion is enabled.

An entry contains only what is needed for traversal:

- `name`, relative to its parent directory;
- `path`, an absolute path to the entry;
- `level`, where entries in the directory passed to `init` are level zero;
- a file-or-directory classification.

Enumeration order is whatever the operating system returns; it is not sorted or otherwise normalized. If an application
needs stable ordering, ownership of names after advancing, timestamps, sizes, permissions, or link-specific information,
it must copy and post-process entries using its own storage and the appropriate filesystem APIs.

The usual recursive walk is compact:

\snippet Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp walkRecursiveSnippet

`enumerateNext()` returns an invalid SC::Result both when iteration is finished and when traversal fails. The loop must
therefore be followed by `checkErrors()` as above. Omitting that final check can mistake an inaccessible directory, an
overlong path, or exhausted recursion storage for successful completion.

# Choosing recursion explicitly

Set `options.recursive` before iteration for automatic depth-first traversal. For pruning, leave it `false` and call
SC::FileSystemIterator::recurseSubdirectory only when the current entry is a directory worth entering:

\snippet Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp walkRecursiveManualSnippet

Manual recursion is immediate: after `recurseSubdirectory()` succeeds, the next enumeration step starts inside that
directory. It avoids visiting excluded subtrees and can be substantially cheaper than filtering entries after a complete
recursive walk. Calling `recurseSubdirectory()` while automatic recursion is enabled is an error.

# Memory, paths, and lifetime

The library performs no dynamic allocation. That choice makes its resource limits visible to the caller:

- The `FolderState` span passed to `init` needs one element for the root plus one for every simultaneously open nested
  directory. In a recursive walk, its length is therefore the maximum supported directory depth plus one. Exhausting it
  stops traversal and is reported by `checkErrors()`.
- Path assembly uses fixed-capacity SC::StringPath storage. A path that does not fit produces an error rather than an
  allocation.
- `Entry::name` and `Entry::path` are borrowed views into iterator or operating-system storage. Use them before the next
  `enumerateNext()` or `init()` call, or copy them into caller-owned storage.
- Destroying or reinitializing the iterator closes any directory handles still held by an unfinished walk.

On POSIX systems, input and output paths use UTF-8; UTF-16 input is rejected. On Windows, input may be UTF-8 or UTF-16,
and returned entry strings use the native UTF-16 encoding. Windows paths use backslashes by default; set
`options.forwardSlashes` to request forward slashes in returned `Entry::path` values. The iterator handles Windows long
path transport internally, but exposes logical paths without the `\\?\` transport prefix.

# Where it fits

Choose File System Iterator when the job is to stream names from a directory tree with predictable storage and decide
what to do with each entry. Neighboring libraries cover different jobs:

- [File System](@ref library_file_system) creates, copies, removes, renames, and queries paths. It complements traversal
  when an entry needs a filesystem operation or richer metadata.
- [File](@ref library_file) opens file contents for reading, writing, and seeking; this iterator does not open returned
  files.
- [File System Watcher](@ref library_file_system_watcher) reports changes over time. File System Iterator is a snapshot-like
  walk initiated by the caller, not a subscription to future changes.
- [Strings](@ref library_strings) provides SC::Path and string operations for parsing, composing, filtering, or converting
  the returned native-encoding paths.

The implementation intentionally exposes a small synchronous pull interface. It does not provide asynchronous
enumeration, parallel traversal, cycle detection, sorting, globbing, or a persistent collection. Filesystem contents can
also change during a walk, so consumers that require an atomic snapshot need stronger guarantees than this API offers.

# Status and examples

🟩 **Usable.** The core immediate, automatic-recursive, and manually pruned traversal paths are covered by
`FileSystemIteratorTest`. The library is also used by `SC-format` to discover source files in the repository.

Relevant development notes:

- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)
- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)

# API reference

@copydoc SC::FileSystemIterator

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/FileSystemIterator`.
Single File counts
`SaneCppFileSystemIterator.h`.
Standalone counts `SaneCppFileSystemIteratorStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 92		| 402		| 494	|
| Single File | 1121		| 977		| 2098	|
| Standalone  | 1121		| 977		| 2098	|
