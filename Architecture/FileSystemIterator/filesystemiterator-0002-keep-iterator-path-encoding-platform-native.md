# FILESYSTEMITERATOR-0002 - Keep Iterator Path Encoding Platform-Native

Status: Accepted
Date: 2026-07-04

## Context

Directory enumeration returns names and paths that often flow directly back into native filesystem APIs. Forcing a single cross-platform text encoding at the iterator boundary would require conversion storage, introduce failure modes, and blur Windows UTF-16 behavior versus POSIX byte-oriented paths.

## Decision

`FileSystemIterator` keeps path encoding platform-native. On Windows it accepts UTF-8 or UTF-16 input paths and returns UTF-16 paths. On POSIX it accepts and returns UTF-8 paths and rejects UTF-16 input. Windows long-path transport prefixes are internal implementation details and are not returned as public iterator paths.

## Consequences

Callers can pass iterator results back to native path-taking APIs without a mandatory conversion layer. Cross-platform callers must account for platform-native encoding in returned `StringSpan` values. The iterator avoids pulling in higher-level string conversion or allocation-capable string ownership.

## Confirmation

A change preserves this decision when documentation continues to state the per-platform encoding behavior, POSIX rejects UTF-16 input, Windows tests ensure prefixed input produces logical output, and iterator paths are represented with `StringSpan`/`StringPath` rather than allocation-owning string types.

## Related

- [FileSystemIterator documentation](../../Documentation/Libraries/FileSystemIterator.md)
- [FileSystemIterator public API](../../Libraries/FileSystemIterator/FileSystemIterator.h)
- [POSIX iterator backend](../../Libraries/FileSystemIterator/Internal/FileSystemIteratorPosix.inl)
- [Windows iterator backend](../../Libraries/FileSystemIterator/Internal/FileSystemIteratorWindows.inl)
- [FileSystemIterator tests](../../Tests/Libraries/FileSystemIterator/FileSystemIteratorTest.cpp)
- [COMMON-0008 - Keep StringSpan and StringPath in Common](../Common/common-0008-keep-stringspan-and-stringpath-in-common.md)
