# Watcher Events And Limits

## Start Here

- `Documentation/Libraries/FileSystemWatcher.md`
- `Libraries/FileSystemWatcher/FileSystemWatcher.h`
- `Tests/Libraries/FileSystemWatcher/FileSystemWatcherTest.cpp`
- `Tests/Libraries/FileSystemWatcher/FileSystemWatcherAsyncTest.cpp`
- `Examples/SCExample/HotReloadSystem.h`

## Use `filesystem-watcher` For

- Watching a directory for add, remove, rename, or modify events.
- Building hot-reload or cache-invalidation flows.
- Feeding change notifications into an async backend.

## Important Behavior

- On macOS and iOS the backend uses `FSEvents`.
- On Windows the backend uses `ReadDirectoryChangesW`.
- On iOS the underlying API is private and may be App Store sensitive.

## Common Patterns

- Seed the watcher from paths discovered with `filesystem-iterator`.
- Use the async template when you already have an event loop.
- Route hot-reload state changes through `plugin-build` when the watcher drives plugin reloads.

## Hand Off To Other Skills

- Use `async` for event-loop integration.
- Use `filesystem` for the mutation that happens after a watched event.
- Use `plugin-build` for reload-oriented compositions.

## Pitfalls

- Do not describe it as polling.
- Do not assume identical behavior across filesystems or platforms.
- Do not bury the backend choice when the user needs portability guidance.
