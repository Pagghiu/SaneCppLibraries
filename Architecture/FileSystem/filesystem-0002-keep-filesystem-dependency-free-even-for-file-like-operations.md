# FILESYSTEM-0002 - Keep FileSystem Dependency-Free Even for File-Like Operations

Status: Accepted
Date: 2026-07-04

## Context

`FileSystem` provides path-based operations that overlap with descriptor-oriented facilities in `File`, such as reading, writing, copying, stat metadata, permissions, and timestamps. Depending on `File`, Time, Strings, or allocation-capable helpers would shrink implementation code but would make a basic filesystem library pull in unrelated library surfaces and weaken standalone single-file consumption.

## Decision

`FileSystem` remains dependency-free at the library graph level. It owns its path-based file-like operations directly, uses Common primitives such as `StringSpan`, `StringPath`, `IGrowableBuffer`, `Result`, and `TimeMs`, and calls native OS APIs from its own implementation instead of reaching through higher-level Sane libraries.

## Consequences

Some native I/O, copy, stat, and permission logic is duplicated or shaped differently from `File`. In exchange, users can adopt `FileSystem` alone, dependency reports stay simple, and file-like path operations do not force descriptor APIs, allocation-capable string ownership, or time-library policy into the library.

## Confirmation

A change preserves this decision when `Support/Dependencies/Dependencies.json` still reports no direct dependencies for `FileSystem`, path-based read/write/copy/stat/permission tests remain in `Tests/Libraries/FileSystem`, and new helpers use Common fragments or native implementation code instead of adding dependencies on `File`, `Strings`, `Memory`, or `Time`.

## Related

- [FileSystem documentation](../../Documentation/Libraries/FileSystem.md)
- [FileSystem public API](../../Libraries/FileSystem/FileSystem.h)
- [FileSystem implementation](../../Libraries/FileSystem/FileSystem.cpp)
- [Dependency metadata](../../Support/Dependencies/Dependencies.json)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [COMMON-0008 - Keep StringSpan and StringPath in Common](../Common/common-0008-keep-stringspan-and-stringpath-in-common.md)
