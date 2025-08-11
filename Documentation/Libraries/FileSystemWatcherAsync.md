@page library_file_system_watcher_async File System Watcher Async

@brief 🟩 [Async](@ref library_async) backend for [FileSystemWatcher](@ref library_file_system_watcher)

[TOC]

SC::FileSystemWatcherAsync is an implementation of SC::FileSystemWatcher that uses SC::Async to deliver notifications.

# Dependencies
- Direct dependencies: [Async](@ref library_async), [FileSystemWatcher](@ref library_file_system_watcher), [Foundation](@ref library_foundation)
- All dependencies: [Async](@ref library_async), [File](@ref library_file), [FileSystem](@ref library_file_system), [FileSystemWatcher](@ref library_file_system_watcher), [Foundation](@ref library_foundation), [Socket](@ref library_socket), [Threading](@ref library_threading), [Time](@ref library_time)

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 29			| 37		| 66	|
| Sources   | 77			| 23		| 100	|
| Sum       | 106			| 60		| 166	|

# Features
- Implement SC::FileSystemWatcher::EventLoopRunner for macOS, Windows and Linux

# Status
🟩 Usable  
Library does have basic capabilities and it can be used just fine.

# Blog

Some relevant blog posts are:

- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)

# Roadmap

🟦 Complete Features:
- Not sure what else could be useful here

💡 Unplanned Features:
- Not sure what else could be useful here
