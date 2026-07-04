# FILESYSTEMITERATOR-0001 - Make Recursive Enumeration Caller-Bounded

Status: Accepted
Date: 2026-07-04

## Context

Recursive directory enumeration needs per-directory state. Automatically allocating an unbounded stack would violate the project's no-hidden-allocation rule, while callback-based enumeration would make large trees harder to pause, filter, or integrate into caller-controlled loops.

## Decision

`FileSystemIterator` uses a pull iterator API and stores recursive traversal state in a caller-provided `Span<FileSystemIterator::FolderState>`. Callers choose the recursion budget by sizing that span. Automatic recursion and manual `recurseSubdirectory` both consume the same bounded state.

## Consequences

Directory walking remains allocation-free and can be paused between `enumerateNext` calls. Deep trees can fail when the caller-provided recursion storage is exhausted, and callers must check `checkErrors` after enumeration to distinguish normal completion from traversal errors.

## Confirmation

A change preserves this decision when `FileSystemIterator::init` still requires caller-provided `FolderState` storage, recursive traversal does not allocate hidden dynamic memory, manual recursion remains available, and tests cover insufficient recursion storage through `checkErrors`.

## Related

- [FileSystemIterator documentation](../../Documentation/Libraries/FileSystemIterator.md)
- [FileSystemIterator public API](../../Libraries/FileSystemIterator/FileSystemIterator.h)
- [POSIX iterator backend](../../Libraries/FileSystemIterator/Internal/FileSystemIteratorPosix.inl)
- [Windows iterator backend](../../Libraries/FileSystemIterator/Internal/FileSystemIteratorWindows.inl)
- [FileSystemIterator tests](../../Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
