[![Windows](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/windows.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/windows.yml)
[![macOS x64](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/macos_x64.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/macos_x64.yml)
[![Linux](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/linux.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/linux.yml)

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

Blog posts:
- [March 2024 Update](https://pagghiu.github.io/site/blog/2024-03-27-SaneCppLibrariesUpdate.html)
- [February 2024 Update](https://pagghiu.github.io/site/blog/2024-02-23-SaneCppLibrariesUpdate.html)
- [January 2024 Update](https://pagghiu.github.io/site/blog/2024-01-23-SaneCppLibrariesUpdate.html)
- [Public Release blog post](https://pagghiu.github.io/site/blog/2023-12-23-SaneCppLibrariesRelease.html)

On the [Youtube Channel](https://www.youtube.com/@Pagghiu) you can find some videos showing some bits of the development process:
- [Creating a Makefile](https://www.youtube.com/watch?v=2ccW8TBAWWE)
- [Add Makefile backend to SC::Build](https://www.youtube.com/watch?v=wYmT3xAzMxU)
- [Start Linux Porting](https://www.youtube.com/watch?v=DUZeu6VDGL8)
- [Build Everything on Linux](https://www.youtube.com/watch?v=gu3x3Y1zZLI)
- [Posix fork](https://www.youtube.com/watch?v=-OiVELMxL6Q)
- [SC::Async Linux epoll 1/2](https://www.youtube.com/watch?v=4rC4aKCD0V8)
- [SC::Async Linux epoll 2/2](https://www.youtube.com/watch?v=uCsGpJcF2oc)
- [SC::FileSystemWatcher Linux inotify implementation](https://www.youtube.com/watch?v=92saVDCRnCI)
- [A Tour of SC::Async](https://www.youtube.com/watch?v=pIGosb2D2Ro)
- [Linux Async I/O using io_uring (1 of 2)](https://www.youtube.com/watch?v=YR935rorb3E)
- [Linux Async I/O using io_uring (2 of 2)](https://www.youtube.com/watch?v=CgYE0YrpHt0)
- [Simple ThreadPool](https://www.youtube.com/watch?v=e48ruImESxI)
- [Async file read and writes using Thread Pool](https://www.youtube.com/watch?v=WF9beKyEA_E)

# Libraries

Many libraries are in draft state, while others are slightly more usable.  
Click on specific page each library to know about its status.  

- 🟥 Draft (incomplete, work in progress, proof of concept, works on basic case)
- 🟨 MVP (minimum set of features have been implemented)
- 🟩 Usable (a reasonable set of features has been implemented to make library useful)
- 🟦 Complete (all planned features have been implemented)

Library                                                                                                 | Description
:-------------------------------------------------------------------------------------------------------|:------------------------------------------------------------------------------------
[Algorithms](https://pagghiu.github.io/SaneCppLibraries/library_algorithms.html)                        | 🟥 Placeholder library templated algorithms will be placed
[Async](https://pagghiu.github.io/SaneCppLibraries/library_async.html)                                  | 🟨 Async I/O (files, sockets, timers, processes, fs events, threads wake-up)
[Build](https://pagghiu.github.io/SaneCppLibraries/library_build.html)                                  | 🟥 Minimal build system where builds are described in C++
[Containers](https://pagghiu.github.io/SaneCppLibraries/library_containers.html)                        | 🟨 Generic containers (SC::Vector, SC::SmallVector, SC::Array etc.)
[File](https://pagghiu.github.io/SaneCppLibraries/library_file.html)                                    | 🟩 Synchronous Disk File I/O
[File System](https://pagghiu.github.io/SaneCppLibraries/library_file_system.html)                      | 🟩 File System operations { exists | copy | delete } for { files | directories }
[File System Iterator](https://pagghiu.github.io/SaneCppLibraries/library_file_system_iterator.html)    | 🟩 Enumerates files and directories inside a given path
[File System Watcher](https://pagghiu.github.io/SaneCppLibraries/library_file_system_watcher.html)      | 🟩 Notifications {add, remove, rename, modified} for files and directories
[Foundation](https://pagghiu.github.io/SaneCppLibraries/library_foundation.html)                        | 🟩 Primitive types, asserts, limits, Function, Span, Result, Tagged Union
[Hashing](https://pagghiu.github.io/SaneCppLibraries/library_hashing.html)                              | 🟩 Compute `MD5`, `SHA1` or `SHA256` hashes for a stream of bytes
[Http](https://pagghiu.github.io/SaneCppLibraries/library_http.html)                                    | 🟥 HTTP parser, client and server
[Plugin](https://pagghiu.github.io/SaneCppLibraries/library_plugin.html)                                | 🟥 Minimal dependency based plugin system with hot-reload
[Process](https://pagghiu.github.io/SaneCppLibraries/library_process.html)                              | 🟩 Create child processes and chain them (also usable with [Async](https://pagghiu.github.io/SaneCppLibraries/library_async.html) library)
[Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html)                        | 🟩 Describe C++ types at compile time for serialization
[Serialization Binary](https://pagghiu.github.io/SaneCppLibraries/library_serialization_binary.html)    | 🟨 Serialize to and from a binary format using [Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html)
[Serialization Text](https://pagghiu.github.io/SaneCppLibraries/library_serialization_text.html)        | 🟨 Serialize to / from text formats (JSON) using [Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html)
[Socket](https://pagghiu.github.io/SaneCppLibraries/library_socket.html)                                | 🟨 Synchronous socket networking and DNS lookup
[Strings](https://pagghiu.github.io/SaneCppLibraries/library_strings.html)                              | 🟩 String formatting / conversion / manipulation (ASCII / UTF8 / UTF16)
[Testing](https://pagghiu.github.io/SaneCppLibraries/library_testing.html)                              | 🟨 Simple testing framework used by all of the other libraries
[Threading](https://pagghiu.github.io/SaneCppLibraries/library_threading.html)                          | 🟥 Atomic, thread, thread pool, mutex, condition variable
[Time](https://pagghiu.github.io/SaneCppLibraries/library_time.html)                                    | 🟨 Time handling (relative, absolute, high resolution)

# C Bindings
Some Libraries have C bindings

Binding                                                                                 | Description
:---------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------
[sc_hashing](https://pagghiu.github.io/SaneCppLibraries/group__group__sc__hashing.html) | Bindings for the [Hashing](https://pagghiu.github.io/SaneCppLibraries/library_hashing.html) Library


# Building

Libraries can be used as is, adding a single file to your project and without needing any build system.  
See [Building (user)](https://pagghiu.github.io/SaneCppLibraries/page_building_user.html) to just use the library

Shortly:
- Add [SC.cpp](Bindings/cpp/SC.cpp) to your build system of choice
- Define `SC_COMPILER_ENABLE_STD_CPP=1` if you plan to use the Standard C++ library
- Include any public header (`Libraries/[Library]/*.h`)

## macOS
- Link `CoreFoundation.framework`
- Link `CoreFoundation.framework`

## Linux
- Link `libdl` (`-ldl`)
- Link `libpthread` (`-lpthread`)

## Windows
- Nothing else to link (in addition to default libs)

# Examples

Check the [Examples](https://pagghiu.github.io/SaneCppLibraries/page_examples.html) page.

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

# Contributing

Please take some time to read the [Principles](https://pagghiu.github.io/SaneCppLibraries/page_principles.html) and [Coding Style](https://pagghiu.github.io/SaneCppLibraries/page_coding_style.html).

After that you can read the [CONTRIBUTING.md](CONTRIBUTING.md) guide.


# License

Sane C++ Libraries are licensed under the MIT License, see [LICENSE.txt](LICENSE.txt) for more information.
