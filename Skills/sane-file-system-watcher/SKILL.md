---
name: sane-file-system-watcher
description: Directory change notifications for files and folders. Use when an AI agent needs to react to add, remove, rename, or modify events, including hot-reload style workflows in Sane C++.
---

# Sane File System Watcher

## Overview

Use this skill for watching directory changes and turning them into notifications or callbacks. Keep it focused on events and backends; use `sane-async` only when integrating with the async watcher template.

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

- Use `sane-file-system` or `sane-file-system-iterator` to seed the paths you want to watch.
- Use `sane-async` when the watcher is part of a broader event loop.
- Use `sane-plugin-build-recipes` if the goal is hot reload for plugins or toolchains.

## Pitfalls

- Do not present it as a polling library.
- Do not treat platform behavior as identical across filesystems.
- Do not hide the event backend from the user-facing explanation.

## References

- [Watcher reference](references/watcher-events-and-limits.md)
