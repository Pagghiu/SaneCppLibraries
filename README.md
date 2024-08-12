[![Windows](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/windows.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/windows.yml)
[![Linux+macOS](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/posix.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/posix.yml)
[![Coverage](https://pagghiu.github.io/SaneCppLibraries/coverage/coverage.svg)](https://pagghiu.github.io/SaneCppLibraries/coverage)

# Sane C++ Libraries

[![YouTube](https://img.shields.io/youtube/channel/subscribers/UCnmN_whfM12LU6VNQWG0NFg)](https://youtube.com/@Pagghiu)
[![X (formerly Twitter) Follow](https://img.shields.io/twitter/follow/pagghiu_)](https://twitter.com/pagghiu_)
[![Discord](https://img.shields.io/discord/1195076118307426384)](https://discord.gg/tyBfFp33Z6)
![GitHub Repo stars](https://img.shields.io/github/stars/Pagghiu/SaneCppLibraries)

**Sane C++ Libraries** is a set of C++ platform abstraction libraries for macOS, Windows and Linux.

![Sane Cpp](https://pagghiu.github.io/site/blog/2023-12-23-SaneCppLibrariesRelease/article.svg)

[Principles](https://pagghiu.github.io/SaneCppLibraries/page_principles.html):

✅ Fast compile times  
✅ Bloat free  
✅ Simple readable code  
✅ Easy to integrate  
⛔️ No C++ Standard Library / Exceptions / RTTI  
⛔️ No third party dependencies (prefer OS API)

Visit the [documentation website](https://pagghiu.github.io/SaneCppLibraries/index.html) for more information.

# Libraries

Library                                                                                                 | Description
:-------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------
[Algorithms](https://pagghiu.github.io/SaneCppLibraries/library_algorithms.html)                        | 🟥 Placeholder library where templated algorithms will be placed
[Async](https://pagghiu.github.io/SaneCppLibraries/library_async.html)                                  | 🟨 Async I/O (files, sockets, timers, processes, fs events, threads wake-up)
[Build](https://pagghiu.github.io/SaneCppLibraries/library_build.html)                                  | 🟨 Minimal build system where builds are described in C++
[Containers](https://pagghiu.github.io/SaneCppLibraries/library_containers.html)                        | 🟨 Generic containers (SC::Vector, SC::SmallVector, SC::Array etc.)
[File](https://pagghiu.github.io/SaneCppLibraries/library_file.html)                                    | 🟩 Synchronous Disk File I/O
[File System](https://pagghiu.github.io/SaneCppLibraries/library_file_system.html)                      | 🟩 File System operations { exists, copy, delete } for { files and directories }
[File System Iterator](https://pagghiu.github.io/SaneCppLibraries/library_file_system_iterator.html)    | 🟩 Enumerates files and directories inside a given path
[File System Watcher](https://pagghiu.github.io/SaneCppLibraries/library_file_system_watcher.html)      | 🟩 Notifications {add, remove, rename, modified} for files and directories
[Foundation](https://pagghiu.github.io/SaneCppLibraries/library_foundation.html)                        | 🟩 Primitive types, asserts, limits, Function, Span, Result, Tagged Union
[Hashing](https://pagghiu.github.io/SaneCppLibraries/library_hashing.html)                              | 🟩 Compute `MD5`, `SHA1` or `SHA256` hashes for a stream of bytes
[Http](https://pagghiu.github.io/SaneCppLibraries/library_http.html)                                    | 🟥 HTTP parser, client and server
[Plugin](https://pagghiu.github.io/SaneCppLibraries/library_plugin.html)                                | 🟨 Minimal dependency based plugin system with hot-reload
[Process](https://pagghiu.github.io/SaneCppLibraries/library_process.html)                              | 🟩 Create child processes and chain them (also usable with [Async](https://pagghiu.github.io/SaneCppLibraries/library_async.html) library)
[Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html)                        | 🟩 Describe C++ types at compile time for serialization
[Serialization Binary](https://pagghiu.github.io/SaneCppLibraries/library_serialization_binary.html)    | 🟨 Serialize to and from a binary format using [Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html)
[Serialization Text](https://pagghiu.github.io/SaneCppLibraries/library_serialization_text.html)        | 🟨 Serialize to / from text formats (JSON) using [Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html)
[Socket](https://pagghiu.github.io/SaneCppLibraries/library_socket.html)                                | 🟨 Synchronous socket networking and DNS lookup
[Strings](https://pagghiu.github.io/SaneCppLibraries/library_strings.html)                              | 🟩 String formatting / conversion / manipulation (ASCII / UTF8 / UTF16)
[Testing](https://pagghiu.github.io/SaneCppLibraries/library_testing.html)                              | 🟨 Simple testing framework used by all of the other libraries
[Threading](https://pagghiu.github.io/SaneCppLibraries/library_threading.html)                          | 🟥 Atomic, thread, thread pool, mutex, condition variable
[Time](https://pagghiu.github.io/SaneCppLibraries/library_time.html)                                    | 🟨 Time handling (relative, absolute, high resolution)

Each library is color-coded to signal its status:
- 🟥 Draft (incomplete, work in progress, proof of concept, works on basic case)
- 🟨 MVP (minimum set of features have been implemented)
- 🟩 Usable (a reasonable set of features has been implemented to make library useful)
- 🟦 Complete (all planned features have been implemented)


# C Bindings
Some Libraries have C bindings

Binding                                                                                 | Description
:---------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------
[sc_hashing](https://pagghiu.github.io/SaneCppLibraries/group__group__sc__hashing.html) | Bindings for the [Hashing](https://pagghiu.github.io/SaneCppLibraries/library_hashing.html) Library


# Building

Libraries can be used as is, adding a single file to your project and without needing any build system.  
See [Building (user)](https://pagghiu.github.io/SaneCppLibraries/page_building_user.html) to just use the library

Shortly:
- Add [Bindings/cpp/SC.cpp](Bindings/cpp/SC.cpp) to your build system of choice
- Define `SC_COMPILER_ENABLE_STD_CPP=1` if you plan to use the Standard C++ library
- Include any public header (`Libraries/[Library]/*.h`)

## Windows
- Nothing else to link (in addition to default libs)

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
- [Twitter](https://twitter.com/pagghiu_) `@pagghiu_`
- [Mastodon](https://mastodon.gamedev.place/@pagghiu) `@pagghiu`
- [Github Discussion](https://github.com/Pagghiu/SaneCppLibraries/discussions)

Alternatively I am also reading the following discords too:
- [Italian C++ Discord](https://discord.gg/GPATr8QxfS) (`@Pagghiu` from any appropriate channel or just a DM, english and italian are both fine)
- [Handmade Network discord](https://discord.gg/hmn) (`@Pagghiu` from any appropriate channel or just a DM)

# Videos

Blog posts:

You can find some [YouTube Videos](https://www.youtube.com/@Pagghiu) and [Blog posts](https://pagghiu.github.io) describing some bits of the development process:

- [Ep.01 - Public Release](https://pagghiu.github.io/site/blog/2023-12-23-SaneCppLibrariesRelease.html)
- [Ep.02 - Creating a Makefile](https://www.youtube.com/watch?v=2ccW8TBAWWE)
- [Ep.03 - Add Makefile backend to SC::Build](https://www.youtube.com/watch?v=wYmT3xAzMxU)
- [Ep.04 - Start Linux Porting](https://www.youtube.com/watch?v=DUZeu6VDGL8)
- [Ep.05 - Build Everything on Linux](https://www.youtube.com/watch?v=gu3x3Y1zZLI)
- [Ep.06 - Posix fork](https://www.youtube.com/watch?v=-OiVELMxL6Q)
- [Ep.07 - SC::Async Linux epoll 1/2](https://www.youtube.com/watch?v=4rC4aKCD0V8)
- [Ep.08 - SC::Async Linux epoll 2/2](https://www.youtube.com/watch?v=uCsGpJcF2oc)
- [Ep.09 - SC::FileSystemWatcher Linux inotify implementation](https://www.youtube.com/watch?v=92saVDCRnCI)
- [January 2024 Update](https://pagghiu.github.io/site/blog/2024-01-23-SaneCppLibrariesUpdate.html)

- [Ep.10 - A Tour of SC::Async](https://www.youtube.com/watch?v=pIGosb2D2Ro)
- [Ep.11 - Linux Async I/O using io_uring (1 of 2)](https://www.youtube.com/watch?v=YR935rorb3E)
- [Ep.12 - Linux Async I/O using io_uring (2 of 2)](https://www.youtube.com/watch?v=CgYE0YrpHt0)
- [Ep.13 - Simple ThreadPool](https://www.youtube.com/watch?v=e48ruImESxI)
- [February 2024 Update](https://pagghiu.github.io/site/blog/2024-02-23-SaneCppLibrariesUpdate.html)

- [Ep.14 - Async file read and writes using Thread Pool](https://www.youtube.com/watch?v=WF9beKyEA_E)
- [March 2024 Update](https://pagghiu.github.io/site/blog/2024-03-27-SaneCppLibrariesUpdate.html)

- [Ep.16 - Implement SC::AsyncLoopWork](https://www.youtube.com/watch?v=huavEjzflHQ)
- [April 2024 Update](https://pagghiu.github.io/site/blog/2024-04-27-SaneCppLibrariesUpdate.html)

- [Ep.18 - BREAK SC::Async IO Event Loop](https://www.youtube.com/watch?v=3lbyx11qDxM)
- [May 2024 Update](https://pagghiu.github.io/site/blog/2024-05-31-SaneCppLibrariesUpdate.html)

- [Ep.20 - Pause Immediate Mode UI - Save CPU Time](https://www.youtube.com/watch?v=4acqdGcUQnE)
- [Ep.21 - Add Async IO to Immediate Mode GUI](https://www.youtube.com/watch?v=z7QaTa7drFo)
- [Ep.22 - Hot-Reload dear imgui](https://www.youtube.com/watch?v=BXybEWvSpGU)
- [June 2024 Update](https://pagghiu.github.io/site/blog/2024-06-30-SaneCppLibrariesUpdate.html)

- [Ep.24 - Hot-Reload C++ on iOS](https://www.youtube.com/watch?v=6DfykfYCQdY)
- [Ep.25 - C++ Serialization and Reflection (with Hot-Reload)](https://www.youtube.com/watch?v=d7DXxC6xG_A)
- [July 2024 Update](https://pagghiu.github.io/site/blog/2024-07-31-SaneCppLibrariesUpdate.html)

- [Ep.27 - C++ Async Http Web Server](https://www.youtube.com/watch?v=yg438A9Db50)


# Contributing

Please take some time to read the [Principles](https://pagghiu.github.io/SaneCppLibraries/page_principles.html) and [Coding Style](https://pagghiu.github.io/SaneCppLibraries/page_coding_style.html).

After that you can read the [CONTRIBUTING.md](CONTRIBUTING.md) guide.

# License

Sane C++ Libraries are licensed under the MIT License, see [LICENSE.txt](LICENSE.txt) for more information.
