@page library_file_system_watcher File System Watcher

@brief 🟩 Portable, recursive directory-change notifications for files and directories

[TOC]

[SaneCppFileSystemWatcher.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystemWatcher.h)
wraps the native directory-watching facilities on Windows, macOS, Linux, NetBSD, and FreeBSD. It is aimed at systems such as hot reloaders,
asset pipelines, and development tools that need to know *that a watched tree changed* and then decide what work to redo.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](FileSystemWatcher.svg)


# When it is a good fit

Use `FileSystemWatcher` when a portable change signal is more valuable than a lossless audit trail. A watcher reports the
watched base path, the affected relative path when the operating system provides one, and one of two deliberately broad
operations:

- `Modified` means contents, timestamps, or other metadata may have changed.
- `AddRemoveRename` means the directory structure changed, without pretending that every backend can distinguish creation,
  deletion, and rename consistently.

This coarse model is intentional. Native backends coalesce, duplicate, reorder, or occasionally drop events, and their
behavior can also vary with the filesystem. Treat a notification as a reason to inspect or rebuild current state, not as a
transaction log. The SCExample hot-reload system follows that rule: it remembers a changed path when possible, but falls
back to a full rescan for structural, ambiguous, or multiple changes.

The library watches directory trees recursively. It is less suitable when every write
must be observed, when old and new rename names must be paired, or when application correctness depends on replaying an
exact sequence of changes. A polling and snapshot/diff design is a better fallback for those requirements.

# Mental model

A `FileSystemWatcher` owns the native monitoring machinery. Each stable-address `FolderWatcher` represents one watched
tree and owns its callback and a copy of the watched path. Initialization selects where callbacks run:

| Delivery model | Callback context | Choose it when |
|:---------------|:-----------------|:---------------|
| `ThreadRunner` | A background watcher thread | The application can synchronize or enqueue work itself |
| `FileSystemWatcherAsyncT<EventLoop>` | The thread driving the supplied event loop | Watcher work belongs with other [Async](@ref library_async) callbacks |

The async adapter is a template so this standalone library does not acquire a dependency on `Async`. Instantiate it with
an event-loop type that supplies the expected wake-up, readiness, and external-completion interfaces.

The normal lifecycle is `init`, one or more `watch` calls, optional `stopWatching` calls, then `close`. A `FolderWatcher`
may be reused after it is stopped. Calling `close` stops all remaining watchers, so explicit per-folder shutdown is not
required when the entire owner is being torn down.

The notification kind guides recovery.

- `Change` identifies a possible filesystem change. An empty relative path can represent changes anywhere under the root.
- `RescanRequired` means the caller should reconcile the tree. Re-register the watch if native coverage was lost, such
  as when a Linux directory arrives after setup.
- `CoverageLimited` means monitoring continues with the limitation described by `Notification::failure`.
- `WatchStopped` means monitoring for the reported root has ended. Inspect `Notification::failure`. The folder
  remains attached until explicitly stopped or the owner is closed. When a shared monitor fails, close and
  reinitialize the owner before starting new watches.

The three status notifications use an empty relative path and the coarse `AddRemoveRename` operation.
An ordinary `Change` does not guarantee complete coverage.

The watched root must be an existing directory. A missing root or a regular file is rejected, and the same
`FolderWatcher` can be used for a later watch. A successful root check does not guarantee that the path cannot change
while the native watch is being set up.

# A representative threaded watch

This source-backed snippet is compiled with `FileSystemWatcherTest`. Notice that the callback does little work and that
all objects involved outlive the active watch.

\snippet Tests/Libraries/FileSystemWatcher/FileSystemWatcherTest.cpp fileSystemWatcherThreadRunnerSnippet

For event-loop delivery, initialize the adapter with an existing loop before passing it to the watcher:

\snippet Tests/Libraries/FileSystemWatcher/FileSystemWatcherAsyncTest.cpp fileSystemWatcherAsyncSnippet

In either model, `Notification::basePath` and `relativePath` are borrowed views valid for the callback. Copy anything that
must survive the callback. `getFullPath` joins them into a caller-owned `StringPath` and can fail if that destination cannot
hold the result.

# Storage, limits, and lifetime

The API does not allocate watcher objects for the caller. `FileSystemWatcher`, its runner, and every `FolderWatcher` contain
their platform storage inline. Their addresses and lifetimes are part of the contract:

- The runner passed to `init` must remain alive and at the same address until `close` completes.
- An active `FolderWatcher` is linked into its owner, so it must remain alive and must not move until `stopWatching` or
  `close` completes. An `ArenaMap` is one option for stable reusable storage.
- The callback and everything it captures must remain valid for the same interval. With `ThreadRunner`, callback state must
  also be safe to access from another thread.
- Check every `Result`. Setup, path copying, backend registration, buffer exhaustion, stopping, and closing can fail.

Apple supports up to 1024 watched paths. Windows `ThreadRunner` supports up to 63 watched roots per owner because
one slot in its native wait set is reserved for control. Each Windows folder has a 16 KiB native change buffer.
Linux installs one `inotify` watch per directory found during setup and tracks up to 128 directories per
`FolderWatcher`. Relative paths are stored in caller-provided memory. The default Linux path store is 4 KiB; pass a larger `Span<char>` to the `FolderWatcher`
constructor for broader or deeper trees. That buffer is ignored on Windows and Apple platforms.

On NetBSD and FreeBSD, each folder initially holds 128 inventory entries, including the root, and 4 KiB of path-component
storage. Regular files and directories each use a native descriptor. To watch larger trees, pass a path-component buffer
and an entry buffer to the two-argument `FolderWatcher` constructor; use
`FolderWatcher::entryStorageSize(numberOfEntries)` to size the latter. The buffers must not overlap and must remain
unchanged until the watch stops. The entry buffer is ignored by other platforms. A setup that exceeds entry capacity or
available descriptors fails and rolls back; exhaustion while monitoring sends `WatchStopped` and requires the owner to
be closed and reinitialized. Native descriptor limits can be reached before the caller's buffer is full.

BSD records symbolic links and other non-regular, non-directory entries without a direct vnode registration. The
watcher does not follow directory symlinks or monitor those entries' contents or metadata directly. It emits a
`CoverageLimited` notification when a watch begins with passive entries or discovers a new passive path; their directory
entry changes can still produce structural hints. Inspect the tree and use another mechanism if direct coverage is
required. A root rename or removal, vnode revocation, failed reconciliation, or fatal queue/readiness error stops the
shared monitor and reports `WatchStopped`. Descendants that disappear during discovery are skipped or retired.
A descendant identity mismatch during reconciliation triggers one retry. A successful retry reports
`RescanRequired` for the root, and a failed retry stops monitoring. A structural change may yield a specific path or
an empty root hint when changes are ambiguous. BSD thread callbacks can stop watches, but closing the owner from its own worker callback fails;
close it from another thread after that callback returns.

If Linux reports a recursive setup failure after registering watches, it attempts to remove all of them. The
`FolderWatcher` can be reused when cleanup succeeds; if a removal fails, `watch` returns the cleanup error.
Linux opens descendants relative to their parent directory and skips directory symlinks during discovery. Registering
the opened directories with inotify requires `/proc/self/fd`. Disappearing entries can be skipped during discovery.
A replacement detected during registration fails setup and rolls back. Restart the watch to restore coverage of
directories created after setup.
When Linux folder watches on the same owner overlap, stopping one folder leaves shared native watches active for the
other folders that still use them.

If the shared Linux inotify queue overflows, each remaining folder receives a `RescanRequired` notification because
individual names were lost. Rescan and restart the watch if recursive coverage must include directories created during
the loss. A fatal inotify read, worker wait or Async readiness failure sends `WatchStopped` to each remaining folder;
the owner rejects new watches until it is closed and reinitialized. Close a Linux ThreadRunner from another thread
after its callback returns.

| Backend | `RescanRequired` | `WatchStopped` |
|:--------|:-----------------|:---------------|
| Windows | Native buffer overflow | Failed directory completion or rearm ends that folder. A worker wait failure ends all roots on the thread runner. |
| Apple | Native drops, event ID wrap, coalesced subtree or mount events, and shared stream refresh | A native root-change event ends the affected folder. |
| Linux | Shared queue overflow, an unrepresentable event path, or a directory tree change | Root invalidation or failed directory registration ends that folder. A fatal read, wait, or readiness failure ends the shared monitor. |
| NetBSD and FreeBSD | Recovered identity mismatch or an unrepresentable event path | Root loss, vnode revocation, failed reconciliation, or a fatal queue/readiness error ends the shared monitor. |

The backend reports the loss it can observe. Filesystem and native API behavior can limit which events arrive.

The underlying operating-system services may allocate their own resources. In particular, the Apple backend uses
CoreServices objects while rebuilding its FSEvents stream. The allocation-free API shape therefore means caller-owned,
bounded SC object storage; it is not a promise that native watcher setup performs no system allocation.

# Platform behavior worth designing for

| Platform | Native mechanism | Practical consequence |
|:---------|:-----------------|:----------------------|
| Windows | `ReadDirectoryChangesW` with subtree watching | Relative paths are UTF-16; buffer overflow loses individual names and produces a conservative root notification |
| macOS | FSEvents with per-file events | Events can be coalesced; stream setup has roughly 200 ms latency and structural distinctions remain intentionally coarse |
| Linux | `inotify`, one watch per directory discovered during setup | Paths must be ASCII or UTF-8; recursive capacity and stored relative paths are bounded as described above |
| NetBSD, FreeBSD | `kqueue`, with a bounded inventory of directory and regular-file vnodes | Directory changes are reconciled recursively; descriptor limits and passive-entry notifications apply |

Changes that bypass the watched directory entry may not produce a notification. Examples include modifying a file through
a symbolic or hard link outside the watched tree, and some memory-mapped writes. On Linux, the watcher registers new or
moved-in subdirectories recursively and then reports `RescanRequired` for changes that may have happened before
registration. On every platform, callers should tolerate a
coarse rescan signal and races between notification and inspection: by the time a callback runs, the named path may already
have changed again or disappeared.

@warning On iOS, FSEvents is a private API. Shipping an app that uses this library is therefore likely to be rejected by
the App Store.

# How it fits with neighboring libraries

`FileSystemWatcher` only reports change hints. Use [FileSystem](@ref library_file_system) to query, create, remove, or
rename paths after a notification, and [FileSystemIterator](@ref library_file_system_iterator) to rescan a tree when an
event is ambiguous or names were lost. Use [Strings](@ref library_strings) `Path` operations to inspect or compose paths;
`Notification::getFullPath` is the convenient join when a `StringPath` is already appropriate.

The [Plugin](@ref library_plugin) library is a common consumer rather than a dependency. SCExample combines Plugin,
FileSystemWatcher, and Async into a hot-reload loop: watcher events mark work as pending, a later phase rescans or rebuilds,
and plugin replacement happens outside the notification callback. That separation is useful beyond plugins because it
keeps backend callbacks short and absorbs duplicate bursts.

# Status and further material

🟩 **Usable on the supported desktop platforms.** NetBSD and FreeBSD ARM64 support is experimental.
The threaded and event loop modes pass the shared suite on both tested guests. The watcher reports coarse events
and uses bounded storage for recursive watches. There is no portable polling fallback.

- [SCExample](@ref page_examples) contains the production-shaped hot-reload consumer.
- [Linux inotify implementation video](https://www.youtube.com/watch?v=92saVDCRnCI) discusses one backend.
- Development updates: [June 2024](https://pagghiu.github.io/site/blog/2024-06-30-SaneCppLibrariesUpdate.html),
  [June 2025](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html),
  [July 2025](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html), and
  [March 2026](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html).

Possible future work includes a polling/stat backend for filesystems without usable native notifications and a mode where
the caller supplies the watcher thread. Neither is currently planned as part of the public API.

# Blog

- [September 2026 Update](https://pagghiu.github.io/site/blog/2026-09-30-SaneCppLibrariesUpdate.html)
- [October 2026 Update](https://pagghiu.github.io/site/blog/2026-10-31-SaneCppLibrariesUpdate.html)

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/FileSystemWatcher`.
Single File counts
`SaneCppFileSystemWatcher.h`.
Standalone counts `SaneCppFileSystemWatcherStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 751		| 1224		| 1975	|
| Single File | 1984		| 1651		| 3635	|
| Standalone  | 1984		| 1651		| 3635	|
