# Sane File System Watcher

## Overview

Use this guide for watching directory changes and turning them into notifications or callbacks. Keep it focused on events and backends; use `async` only when integrating with the async watcher template.

## Start Here

- Inspect `Documentation/Libraries/FileSystemWatcher.md`.
- Read `Tests/Libraries/FileSystemWatcher/FileSystemWatcherTest.cpp` and `Tests/Libraries/FileSystemWatcher/FileSystemWatcherAsyncTest.cpp`.
- Check `Libraries/FileSystemWatcher/FileSystemWatcher.h`.

## Use It For

- Watch directories for add, remove, rename, or modified events.
- Build hot-reload or cache invalidation flows.
- Bridge change notifications into an async event loop with `FileSystemWatcherAsyncT`.

## Platform Notes

- On macOS and iOS the backend uses `FSEvents`.
- On Windows the backend uses `ReadDirectoryChangesW`.
- On iOS the API is private and may cause App Store rejection.

## Prefer These Companions

- Use `filesystem` or `filesystem-iterator` to seed the paths you want to watch.
- Use `async` when the watcher is part of a broader event loop.
- Use `plugin-build` if the goal is hot reload for plugins or toolchains.

## Pitfalls

- Do not present it as a polling library.
- Do not treat platform behavior as identical across filesystems.
- Do not hide the event backend from the user-facing explanation.

## References

- [Watcher reference](references/watcher-events-and-limits.md)
