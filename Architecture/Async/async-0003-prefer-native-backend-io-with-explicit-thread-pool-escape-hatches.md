# ASYNC-0003 - Prefer Native Backend I/O With Explicit Thread-Pool Escape Hatches

Status: Accepted
Date: 2026-07-04

## Context

The platforms supported by `Async` expose different I/O primitives. Some operations are genuinely asynchronous on one backend but may block on another, especially buffered file and filesystem operations. Hiding that difference behind implicit worker allocation would make performance and failure behavior harder to reason about.

## Decision

`Async` prefers native operating-system I/O backends: IOCP on Windows, kqueue on macOS, and direct io_uring on Linux when available, with epoll fallback. Operations that need blocking work use caller-provided `ThreadPool` storage through explicit request configuration. `AsyncThreadPoolMode::ForceThreadPool` remains the visible escape hatch when callers deliberately want the supplied pool even if native async support exists.

## Consequences

Callers may need platform-aware setup for descriptors, thread pools, and backend selection. Linux can use native file I/O through io_uring without liburing as a runtime dependency, while other backends keep blocking work explicit instead of silently creating worker resources.

## Confirmation

A change preserves this decision when backend selection remains explicit and testable, non-native blocking work requires caller-provided `ThreadPool` state, direct io_uring support does not add a liburing dependency, and documentation explains when a thread pool is needed.

## Related

- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [Async documentation: implementation](../../Documentation/Libraries/Async.md)
- [AsyncThreadPoolMode](../../Libraries/Async/Async.h)
