# FileSystemWatcher Architecture

## Purpose

`FileSystemWatcher` owns portable notifications for changes under watched directories. Future work should keep it focused on directory change observation and delivery, not filesystem mutation, polling abstractions, or a general event runtime.

## Architectural Shape

The library wraps native OS watch APIs behind `FileSystemWatcher`, `FolderWatcher`, and two delivery modes. `ThreadRunner` delivers notifications from library-owned background machinery. `EventLoopRunner` lets an external event loop drive notification delivery, and `FileSystemWatcherAsyncT<T_AsyncEventLoop>` is the template bridge for Async integration without an Async dependency.

Notifications deliberately expose a coarse model: `Operation::Modified` or `Operation::AddRemoveRename`, plus `basePath`, `relativePath`, and a helper to build the full path. Backend implementations normalize native events into that model and keep OS-specific details internal.

## Boundaries

`FileSystemWatcher` owns watch registration, watch teardown, and notification delivery. It does not own file creation/removal/copy operations, directory enumeration, descriptor I/O, or event-loop implementation. It should not depend on Async, Threading, FileSystem, File, or platform SDK concepts in public API beyond fixed opaque storage and Common primitives.

## Similarities With Other Libraries

Like the other filesystem libraries, it is dependency-free, native-backed, and Common-primitive based. Like `FileSystemIterator`, it reports directory entries through path spans. Like `File`, it exposes resources that higher layers can compose with Async without making that dependency mandatory.

## Differences From Other Libraries

Unlike `FileSystemIterator`, it observes future changes rather than enumerating current contents. Unlike `FileSystem`, it reports events but does not mutate files or directories. Unlike `File`, it is callback-notification based and may involve background thread or event-loop integration. Unlike Async, it is not itself an event loop.

## Inspirations

The evidenced inspirations are native watch APIs: Windows `ReadDirectoryChangesW`, Apple `FSEvents`, and Linux `inotify`. The Async integration is inspired by the existing `SC::AsyncEventLoop` shape, but it is expressed as a template bridge so the watcher core remains independently consumable.

## Anti-Inspirations

Inference: this library should not expose every native watcher event or flag as public portable API. Inference: it should not become libuv, Boost.Asio, or an Async wrapper. It should not hide backend limitations such as symlink, hard-link, memory-mapped write, overflow, private iOS FSEvents, or filesystem-specific behavior.

## Architectural Choices

Keep native backend code internal. Keep public notifications coarse and portable. Preserve both `ThreadRunner` and `EventLoopRunner` initialization paths. Keep Async support in `FileSystemWatcherAsyncT` rather than a direct dependency or separate bridge library. Keep recursive Linux watch storage bounded and explicit where needed. Document limitations instead of promising exact native event fidelity.

## Explicitly Excluded Targets

Do not add mandatory polling fallback, caller-owned thread injection, exact native event streams, global recursive watch management, or filesystem mutation APIs unless new ADRs define those boundaries. Do not reintroduce `FileSystemWatcherAsync` as a separate dependency-bearing library.

## Sources

- [FileSystemWatcher documentation](../../Documentation/Libraries/FileSystemWatcher.md)
- [FileSystemWatcher public API](../../Libraries/FileSystemWatcher/FileSystemWatcher.h)
- [FileSystemWatcher implementation](../../Libraries/FileSystemWatcher/FileSystemWatcher.cpp)
- [Windows watcher backend](../../Libraries/FileSystemWatcher/Internal/FileSystemWatcherWindows.inl)
- [Apple watcher backend](../../Libraries/FileSystemWatcher/Internal/FileSystemWatcherApple.inl)
- [Linux watcher backend](../../Libraries/FileSystemWatcher/Internal/FileSystemWatcherLinux.inl)
- [Threading adapter used internally](../../Libraries/FileSystemWatcher/Internal/FileSystemWatcherThreading.h)
- [FileSystemWatcher tests](../../Tests/Libraries/FileSystemWatcher/FileSystemWatcherTest.cpp)
- [FileSystemWatcher async tests](../../Tests/Libraries/FileSystemWatcher/FileSystemWatcherAsyncTest.cpp)
- [Dependency metadata](../../Support/Dependencies/Dependencies.json)
- [FILESYSTEMWATCHER-0001 - Keep Async Integration as a Template Bridge](filesystemwatcher-0001-keep-async-integration-as-a-template-bridge.md)
- [FILESYSTEMWATCHER-0002 - Expose Coarse Portable Event Classes Over Native Watch APIs](filesystemwatcher-0002-expose-coarse-portable-event-classes-over-native-watch-apis.md)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)

## Decision Log

- [FILESYSTEMWATCHER-0001 - Keep async integration as a template bridge](filesystemwatcher-0001-keep-async-integration-as-a-template-bridge.md)
- [FILESYSTEMWATCHER-0002 - Expose coarse portable event classes over native watch APIs](filesystemwatcher-0002-expose-coarse-portable-event-classes-over-native-watch-apis.md)
