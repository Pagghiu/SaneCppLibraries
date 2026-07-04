# FILESYSTEMWATCHER-0001 - Keep Async Integration as a Template Bridge

Status: Accepted
Date: 2026-07-04

## Context

`FileSystemWatcher` is useful both with a dedicated background thread and with an existing Async event loop. A direct dependency on Async, or a separate bridge library that depends on both Async and FileSystemWatcher, would make basic filesystem watching pull in the larger Async dependency graph and complicate standalone adoption.

## Decision

`FileSystemWatcher` keeps its core API dependency-free and exposes `EventLoopRunner` as the event-loop integration boundary. `FileSystemWatcherAsyncT<T_AsyncEventLoop>` is a template bridge in the watcher header that composes with Async-like event loops without making Async a library dependency of FileSystemWatcher.

## Consequences

Users can watch files through `ThreadRunner` without adopting Async, while Async users still get first-class integration. Some platform-specific Async adapter code lives in a template, increasing header surface, but it preserves the dependency graph and removes the need for a separate bridge library.

## Confirmation

A change preserves this decision when dependency metadata still reports no direct dependencies for `FileSystemWatcher`, `ThreadRunner` and `EventLoopRunner` remain supported initialization paths, Async tests instantiate `FileSystemWatcherAsyncT<AsyncEventLoop>`, and no standalone `FileSystemWatcherAsync` dependency edge is reintroduced.

## Related

- [FileSystemWatcher documentation](../../Documentation/Libraries/FileSystemWatcher.md)
- [FileSystemWatcher public API](../../Libraries/FileSystemWatcher/FileSystemWatcher.h)
- [FileSystemWatcher implementation](../../Libraries/FileSystemWatcher/FileSystemWatcher.cpp)
- [FileSystemWatcher async tests](../../Tests/Libraries/FileSystemWatcher/FileSystemWatcherAsyncTest.cpp)
- [Dependency metadata](../../Support/Dependencies/Dependencies.json)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
