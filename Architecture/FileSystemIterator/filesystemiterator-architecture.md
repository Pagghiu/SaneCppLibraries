# FileSystemIterator Architecture

## Purpose

`FileSystemIterator` owns allocation-free enumeration of files and directories under a starting directory. Future work should keep it a small pull iterator for directory walking, not a full query language or recursive file operation framework.

## Architectural Shape

The library exposes `FileSystemIterator`, `Entry`, `Options`, and caller-provided `FolderState` storage. Callers initialize the iterator with a directory and a bounded recursion stack, then pull entries with `enumerateNext`. Automatic recursion and manual `recurseSubdirectory` are both supported, and callers must call `checkErrors` after iteration to distinguish normal completion from traversal failure.

Returned path strings are platform-native: Windows outputs UTF-16 logical paths, POSIX outputs UTF-8 paths. The implementation may use transport prefixes or native handles internally, but public entries should remain stable Sane string views valid only until the next iterator movement.

## Boundaries

`FileSystemIterator` owns enumeration and recursion state. It does not create, remove, copy, rename, watch, or mutate filesystem entries. It should not depend on `FileSystem` for traversal or on `Strings`/`Memory` for path storage. Filtering policy belongs to the caller through manual recursion and entry inspection.

## Similarities With Other Libraries

Like the other filesystem-adjacent libraries, it uses native OS APIs internally and Common primitives publicly. Like `FileSystemWatcher`, it returns paths relative to native filesystem observations, but it is synchronous and caller-driven. Like `FileSystem`, it treats platform path encoding as part of the API contract.

## Differences From Other Libraries

Unlike `FileSystem`, the iterator is read-only and traversal-oriented. Unlike `FileSystemWatcher`, it takes snapshots through active enumeration rather than receiving future notifications. Unlike `File`, it has no descriptor I/O surface even though backends may hold directory handles internally.

## Inspirations

The evidenced inspirations are native directory enumeration APIs: POSIX directory descriptors and `readdir`/`openat` style traversal, plus Windows `FindFirstFileW`/`FindNextFileW`. The pull iterator shape is chosen to let callers pace large directory walks and decide recursion policy.

## Anti-Inspirations

Inference: this library should not mimic allocation-heavy recursive directory listing APIs that return all paths at once. Inference: it should not use callback-only traversal that hides pacing and recursion control from the caller. It should not normalize all platforms to a single owned string encoding at the cost of conversion storage.

## Architectural Choices

Keep recursion caller-bounded through `Span<FolderState>`. Keep enumeration pull-based and synchronous. Preserve manual recursion as the mechanism for caller-side filtering. Preserve platform-native path encoding and avoid returning Windows transport prefixes. Report traversal errors through `Result` and `checkErrors` rather than exceptions or hidden state.

## Explicitly Excluded Targets

Do not add hidden allocation, full-tree materialization, globbing, path expression filtering, filesystem mutation, file watching, or dependency on FileSystem. Do not make returned `StringSpan` values outlive the next `enumerateNext`, `recurseSubdirectory`, or `init` call.

## Sources

- [FileSystemIterator documentation](../../Documentation/Libraries/FileSystemIterator.md)
- [FileSystemIterator public API](../../Libraries/FileSystemIterator/FileSystemIterator.h)
- [FileSystemIterator implementation](../../Libraries/FileSystemIterator/FileSystemIterator.cpp)
- [POSIX iterator backend](../../Libraries/FileSystemIterator/Internal/FileSystemIteratorPosix.inl)
- [Windows iterator backend](../../Libraries/FileSystemIterator/Internal/FileSystemIteratorWindows.inl)
- [FileSystemIterator tests](../../Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp)
- [FILESYSTEMITERATOR-0001 - Make Recursive Enumeration Caller-Bounded](filesystemiterator-0001-make-recursive-enumeration-caller-bounded.md)
- [FILESYSTEMITERATOR-0002 - Keep Iterator Path Encoding Platform-Native](filesystemiterator-0002-keep-iterator-path-encoding-platform-native.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [COMMON-0008 - Keep StringSpan and StringPath in Common](../Common/common-0008-keep-stringspan-and-stringpath-in-common.md)

## Decision Log

- [FILESYSTEMITERATOR-0001 - Make recursive enumeration caller-bounded](filesystemiterator-0001-make-recursive-enumeration-caller-bounded.md)
- [FILESYSTEMITERATOR-0002 - Keep iterator path encoding platform-native](filesystemiterator-0002-keep-iterator-path-encoding-platform-native.md)
