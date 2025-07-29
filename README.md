[![Windows](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/windows.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/windows.yml)
[![Linux+macOS](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/posix.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/posix.yml)
[![Coverage](https://pagghiu.github.io/SaneCppLibraries/coverage/coverage.svg)](https://pagghiu.github.io/SaneCppLibraries/coverage)

# Sane C++ Libraries

[![YouTube](https://img.shields.io/youtube/channel/subscribers/UCnmN_whfM12LU6VNQWG0NFg)](https://youtube.com/@Pagghiu)
[![X](https://img.shields.io/twitter/follow/pagghiu_)](https://x.com/pagghiu_)
[![Discord](https://img.shields.io/discord/1195076118307426384)](https://discord.gg/tyBfFp33Z6)
![GitHub Repo stars](https://img.shields.io/github/stars/Pagghiu/SaneCppLibraries)

**Sane C++ Libraries** is a set of C++ platform abstraction libraries for macOS, Windows and Linux.

![Sane Cpp](https://pagghiu.github.io/images/2023-12-23-SaneCppLibrariesRelease/article.svg)

[Principles](https://pagghiu.github.io/SaneCppLibraries/page_principles.html):

✅ Fast compile times  
✅ Bloat free  
✅ Simple and readable code  
✅ Easy to integrate  
⛔️ No C++ Standard Library / Exceptions / RTTI  
⛔️ No third party build dependencies (prefer OS API)

Visit the [documentation website](https://pagghiu.github.io/SaneCppLibraries/index.html) for more information.

Take a look also at [DeepWiki/SaneCppLibraries](https://deepwiki.com/Pagghiu/SaneCppLibraries) for an AI-guided walkthrough of the project!

# Libraries

Each library only depends on the smallest possible subset of the others libraries (see [Dependencies](https://pagghiu.github.io/SaneCppLibraries/page_dependencies.html)).

Library                                                                                                 | Description                                                                                                                                                                                       | LOC
:-------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-----
[Algorithms](https://pagghiu.github.io/SaneCppLibraries/library_algorithms.html)                                | 🟥 Placeholder library where templated algorithms will be placed                                                                                                                          |   102
[Async](https://pagghiu.github.io/SaneCppLibraries/library_async.html)                                          | 🟨 Async I/O (files, sockets, timers, processes, fs events, threads)                                                                                                                      |   5661
[Async Streams](https://pagghiu.github.io/SaneCppLibraries/library_async_streams.html)                          | 🟥 Concurrently read, write and transform byte streams                                                                                                                                    |   2013
[Build](https://pagghiu.github.io/SaneCppLibraries/library_build.html)                                          | 🟨 Minimal build system where builds are described in C++                                                                                                                                 |   4094
[Containers](https://pagghiu.github.io/SaneCppLibraries/library_containers.html)                                | 🟨 Generic containers (SC::Vector, SC::SmallVector, SC::Array etc.)                                                                                                                       |   801
[File](https://pagghiu.github.io/SaneCppLibraries/library_file.html)                                            | 🟩 Synchronous Disk File I/O                                                                                                                                                              |   700
[File System](https://pagghiu.github.io/SaneCppLibraries/library_file_system.html)                              | 🟩 File System operations (like copy / delete) for files / directories                                                                                                                    |   1323
[File System Iterator](https://pagghiu.github.io/SaneCppLibraries/library_file_system_iterator.html)            | 🟩 Enumerates files and directories inside a given path                                                                                                                                   |   417
[File System Watcher](https://pagghiu.github.io/SaneCppLibraries/library_file_system_watcher.html)              | 🟩 Notifications {add, remove, rename, modified} for files / directories                                                                                                                  |   1319
[File System Watcher Async](https://pagghiu.github.io/SaneCppLibraries/library_file_system_watcher_async.html)  | 🟩 [Async](https://pagghiu.github.io/SaneCppLibraries/library_async.html) backend for [File System Watcher](https://pagghiu.github.io/SaneCppLibraries/library_file_system_watcher.html)  |   113
[Foundation](https://pagghiu.github.io/SaneCppLibraries/library_foundation.html)                                | 🟩 Primitive types, asserts, compiler macros, Function, Span, Result                                                                                                                      |   1215
[Hashing](https://pagghiu.github.io/SaneCppLibraries/library_hashing.html)                                      | 🟩 Compute `MD5`, `SHA1` or `SHA256` hashes for a stream of bytes                                                                                                                         |   359
[Http](https://pagghiu.github.io/SaneCppLibraries/library_http.html)                                            | 🟥 HTTP parser, client and server                                                                                                                                                         |   1299
[Memory](https://pagghiu.github.io/SaneCppLibraries/library_memory.html)                                        | 🟩 Custom allocators, Virtual Memory, Buffer, Segment                                                                                                                                     |   1257
[Plugin](https://pagghiu.github.io/SaneCppLibraries/library_plugin.html)                                        | 🟨 Minimal dependency based plugin system with hot-reload                                                                                                                                 |   1464
[Process](https://pagghiu.github.io/SaneCppLibraries/library_process.html)                                      | 🟩 Create child processes and redirect their input / output                                                                                                                                                  |   1318
[Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html)                                | 🟩 Describe C++ types at compile time for serialization                                                                                                                                   |   700
[Serialization Binary](https://pagghiu.github.io/SaneCppLibraries/library_serialization_binary.html)            | 🟨 Serialize to and from a binary format using [Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html)                                                           |   594
[Serialization Text](https://pagghiu.github.io/SaneCppLibraries/library_serialization_text.html)                | 🟨 Serialize to / from text formats (JSON) using [Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html)                                                         |   661
[Socket](https://pagghiu.github.io/SaneCppLibraries/library_socket.html)                                        | 🟨 Synchronous socket networking and DNS lookup                                                                                                                                           |   889
[Strings](https://pagghiu.github.io/SaneCppLibraries/library_strings.html)                                      | 🟩 String formatting / conversion / manipulation (UTF8 / UTF16)                                                                                                                   |   3387
[Testing](https://pagghiu.github.io/SaneCppLibraries/library_testing.html)                                      | 🟨 Simple testing framework used by all of the other libraries                                                                                                                            |   343
[Threading](https://pagghiu.github.io/SaneCppLibraries/library_threading.html)                                  | 🟥 Atomic, thread, thread pool, mutex, condition variable                                                                                                                                 |   895
[Time](https://pagghiu.github.io/SaneCppLibraries/library_time.html)                                            | 🟨 Time handling (relative, absolute, high resolution)                                                                                                                                    |   349

Each library is color-coded to signal its status:  
🟥 Draft (incomplete, WIP, works on basic case)  
🟨 MVP (minimum set of features have been implemented)  
🟩 Usable (a reasonable set of useful features has been implemented)  
🟦 Complete (all planned features have been implemented)  


# C Bindings
Some Libraries have C bindings

Binding                                                                                 | Description
:---------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------
[sc_hashing](https://pagghiu.github.io/SaneCppLibraries/group__group__sc__hashing.html) | Bindings for the [Hashing](https://pagghiu.github.io/SaneCppLibraries/library_hashing.html) Library


# Building

Libraries can be used as is, adding a single file to your project and without needing any build system.  
See [Building (user)](https://pagghiu.github.io/SaneCppLibraries/page_building_user.html) to just use the library

Shortly:
- Add [SC.cpp](SC.cpp) to your build system of choice
- Define `SC_COMPILER_ENABLE_STD_CPP=1` if you plan to use the Standard C++ library
- Include any public header (`Libraries/[Library]/*.h`)

## Windows
- If using MSVC required libraries are already implicitly linked through `#pragma comment(lib, ...)`

## macOS / iOS
- Link `CoreFoundation.framework`
- Link `CoreServices.framework`

## Linux
- Link `libdl` (`-ldl`)
- Link `libpthread` (`-lpthread`)

# Examples

SCExample showcases integration of Sane C++ Libraries together with [Dear ImGui](https://github.com/ocornut/imgui) and [sokol](https://github.com/floooh/sokol) libraries (see [Examples](https://pagghiu.github.io/SaneCppLibraries/page_examples.html) page).

## macOS
https://github.com/user-attachments/assets/2a38310c-6a28-4f86-a0f3-665dc15b126d

## iOS
https://github.com/Pagghiu/SaneCppLibraries/assets/5406873/5c7d4036-6e0c-4262-ad57-9ef84c214717

# Tests

Tests are built with the self-hosted [SC::Build](https://pagghiu.github.io/SaneCppLibraries/library_build.html) project generator, describing the builds in C++.    
Check [Building (contributor)](https://pagghiu.github.io/SaneCppLibraries/page_building_contributor.html) to run the tests.

# Getting in touch

- [Sane Coding Discord](https://discord.gg/tyBfFp33Z6)  
![Discord](https://img.shields.io/discord/1195076118307426384)
- [Bluesky](https://pagghiu.bsky.social) `@pagghiu.bsky.social`
- [X](https://x.com/pagghiu_) `@pagghiu_`
- [Mastodon](https://mastodon.gamedev.place/@pagghiu) `@pagghiu`
- [Github Discussion](https://github.com/Pagghiu/SaneCppLibraries/discussions)

Alternatively I am also reading the following discords too:
- [Italian C++ Discord](https://discord.gg/GPATr8QxfS) (`@Pagghiu` from any appropriate channel or just a DM, english and italian are both fine)
- [Handmade Network discord](https://discord.gg/hmn) (`@Pagghiu` from any appropriate channel or just a DM)

# Contributing

Please take some time to read the [Principles](https://pagghiu.github.io/SaneCppLibraries/page_principles.html) and [Coding Style](https://pagghiu.github.io/SaneCppLibraries/page_coding_style.html).

After that you can read the [CONTRIBUTING.md](CONTRIBUTING.md) guide.

# License

Sane C++ Libraries are licensed under the MIT License, see [LICENSE.txt](LICENSE.txt) for more information.

# Videos

On this [YouTube Channel](https://www.youtube.com/@Pagghiu) there are some videos showing bits of the development process.

# Blog posts

On [Sane Coding Blog](https://pagghiu.github.io) there is a series of posts about this project.

Relevant yearly posts:

- [Blog Post - Sane C++ Libraries (Open Sourcing)](https://pagghiu.github.io/site/blog/2023-12-23-SaneCppLibrariesRelease.html)
- [Blog Post - 1st Year of Sane C++ Libraries](https://pagghiu.github.io/site/blog/2024-12-23-SaneCpp1Year.html)
