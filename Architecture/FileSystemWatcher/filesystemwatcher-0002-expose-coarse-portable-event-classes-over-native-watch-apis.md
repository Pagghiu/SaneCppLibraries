# FILESYSTEMWATCHER-0002 - Expose Coarse Portable Event Classes Over Native Watch APIs

Status: Accepted
Date: 2026-07-04

## Context

Native filesystem watching APIs report different event shapes and guarantees. Windows `ReadDirectoryChangesW`, Apple `FSEvents`, and Linux `inotify` do not agree on rename sequences, directory versus file changes, overflow behavior, symlinks, memory-mapped writes, or modifications made through alternate links. Exposing every backend detail would make the public API non-portable.

## Decision

`FileSystemWatcher` uses native OS watch APIs internally but exposes a coarse portable notification model: `Operation::Modified` and `Operation::AddRemoveRename`, plus the watched base path and relative path. Backend-specific details are normalized or collapsed into those classes, and limitations of directory-entry based watching are documented instead of hidden.

## Consequences

The watcher API is portable enough for common hot-reload and file-change workflows, but it does not promise exact native event fidelity. Some events may be grouped, coalesced, or reported conservatively as add/remove/rename. Users needing backend-specific event streams must use native APIs directly or a future explicitly platform-specific extension.

## Confirmation

A change preserves this decision when public notifications still use the coarse `Operation` enum, backend implementations continue to map native events into those classes, tests assert portable add/remove/rename and modified behavior instead of backend-specific event sequences, and documentation keeps the watcher limitations visible.

## Related

- [FileSystemWatcher documentation](../../Documentation/Libraries/FileSystemWatcher.md)
- [FileSystemWatcher public API](../../Libraries/FileSystemWatcher/FileSystemWatcher.h)
- [Windows watcher backend](../../Libraries/FileSystemWatcher/Internal/FileSystemWatcherWindows.inl)
- [Apple watcher backend](../../Libraries/FileSystemWatcher/Internal/FileSystemWatcherApple.inl)
- [Linux watcher backend](../../Libraries/FileSystemWatcher/Internal/FileSystemWatcherLinux.inl)
- [FileSystemWatcher tests](../../Tests/Libraries/FileSystemWatcher/FileSystemWatcherTest.cpp)
- [FileSystemWatcher async tests](../../Tests/Libraries/FileSystemWatcher/FileSystemWatcherAsyncTest.cpp)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
