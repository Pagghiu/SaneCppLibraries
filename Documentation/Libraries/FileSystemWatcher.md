@page library_file_system_watcher File System Watcher

@brief 🟩 Notifications {add, remove, rename, modified} for files and directories

[TOC]

SC::FileSystemWatcher allows watching directories for changes that happen to them.  

# Dependencies
- Direct dependencies: [Async](@ref library_async), [Foundation](@ref library_foundation), [Threading](@ref library_threading)
- All dependencies: [Async](@ref library_async), [File](@ref library_file), [FileSystem](@ref library_file_system), [Foundation](@ref library_foundation), [Socket](@ref library_socket), [Threading](@ref library_threading), [Time](@ref library_time)

# Statistics
- Lines of code (excluding comments): 1227
- Lines of code (including comments): 1570

# Features
- Get notified about modified files or directories
- Get notified about added / removed / renamed files or directories

# Status
🟩 Usable  
Library does have basic capabilities and it can be used just fine.

# Blog

Some relevant blog posts are:

- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)

# Description

@copydetails SC::FileSystemWatcher

# Videos

This is the list of videos that have been recorded showing some of the internal thoughts that have been going into this library:

- [Ep.09 - SC::FileSystemWatcher Linux inotify implementation](https://www.youtube.com/watch?v=92saVDCRnCI)

# Blog

Some relevant blog posts are:

- [June 2024 Update](https://pagghiu.github.io/site/blog/2024-06-30-SaneCppLibrariesUpdate.html)

# Details

The class tries to unify differences between OS specific API to deliver folder change notifications

- On macOS and iOS `FSEvents` by `CoreServices` is used.  
- On Windows `ReadDirectoryChangesW` is used.  

The behavior between these different system also depends on the file system where the watched directory resides.

@note On iOS `FSEvents` api is private so using SC::FileSystemWatcher will be very likely causing your app to be rejected from the app store.

# Examples

- [SCExample](@ref page_examples) uses SC::FileSystemWatcher for a simple hot-reload system
- Unit test inside `FileSystemWatcherTest.cpp` show how the API is meant to be used

# Roadmap

🟦 Complete Features:
- Not sure what else could be useful here

💡 Unplanned Features:
- Having a thread based polling stat watcher that checks file modifications on intervals as fallback
- Allow users to provide their own thread instead of creating it behind the scenes