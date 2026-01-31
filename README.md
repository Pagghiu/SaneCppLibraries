[![Windows](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/windows.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/windows.yml)
[![Linux+macOS](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/posix.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/posix.yml)
[![Coverage](https://pagghiu.github.io/SaneCppLibraries/coverage/coverage.svg)](https://pagghiu.github.io/SaneCppLibraries/coverage)

# Sane C++ Libraries

[![YouTube](https://img.shields.io/youtube/channel/subscribers/UCnmN_whfM12LU6VNQWG0NFg)](https://youtube.com/@Pagghiu)
[![X](https://img.shields.io/twitter/follow/pagghiu_)](https://x.com/pagghiu_)
[![Discord](https://img.shields.io/discord/1195076118307426384)](https://discord.gg/tyBfFp33Z6)
![GitHub Repo stars](https://img.shields.io/github/stars/Pagghiu/SaneCppLibraries)

**Sane C++ Libraries** is a set of C++ platform abstraction libraries for macOS, Windows and Linux.

[Principles](https://pagghiu.github.io/SaneCppLibraries/page_principles.html):  
✅ Fast compile times  
✅ Bloat free  
✅ Simple and readable code  
✅ Easy to integrate  
⛔️ No C++ Standard Library / Exceptions / RTTI  
⛔️ No third party build dependencies (prefer OS API)

## [Libraries](https://pagghiu.github.io/SaneCppLibraries/libraries.html)

Libraries are designed to be used as [Single File Libraries](https://pagghiu.github.io/SaneCppLibraries/page_single_file_libs.html) with minimal [dependencies](https://pagghiu.github.io/SaneCppLibraries/page_dependencies.html) between them and follow a strict **No Allocations** (*) policy.

Library                                                                                                         | Description                                                               | Single File                                                                                                       
:---------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------|:------------------------------------------------------------------------------------------------------------------
[Async](https://pagghiu.github.io/SaneCppLibraries/library_async.html)                                          | 🟨 Async I/O (files, sockets, timers, processes, fs events, tasks)        | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAsync.h)                   
[Async Streams](https://pagghiu.github.io/SaneCppLibraries/library_async_streams.html)                          | 🟨 Concurrently read, write and transform byte streams                    | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAsyncStreams.h)            
[Containers](https://pagghiu.github.io/SaneCppLibraries/library_containers.html)                                | 🟨 Generic containers (SC::Vector, SC::SmallVector, SC::Array)            | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppContainers.h)              
[Containers Reflection](https://pagghiu.github.io/SaneCppLibraries/library_containers_reflection.html)          | 🟨 Containers specializations for Reflection and Serialization            | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppContainersReflection.h) 
[File](https://pagghiu.github.io/SaneCppLibraries/library_file.html)                                            | 🟩 Synchronous Disk File I/O                                              | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFile.h)                    
[File System](https://pagghiu.github.io/SaneCppLibraries/library_file_system.html)                              | 🟩 File System operations (like copy / delete) for files / dirs           | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystem.h)              
[File System Iterator](https://pagghiu.github.io/SaneCppLibraries/library_file_system_iterator.html)            | 🟩 Enumerates files and directories inside a given path                   | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystemIterator.h)      
[File System Watcher](https://pagghiu.github.io/SaneCppLibraries/library_file_system_watcher.html)              | 🟩 Notifications {add,remove,rename,modify} for files / dirs              | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystemWatcher.h)       
[File System Watcher Async](https://pagghiu.github.io/SaneCppLibraries/library_file_system_watcher_async.html)  | 🟩 Async backend for FileSystemWatcher                                    | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystemWatcherAsync.h)  
[Foundation](https://pagghiu.github.io/SaneCppLibraries/library_foundation.html)                                | 🟩 Primitive types, asserts, macros, Function, Span, Result               | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFoundation.h)              
[Hashing](https://pagghiu.github.io/SaneCppLibraries/library_hashing.html)                                      | 🟩 Compute `MD5`, `SHA1` or `SHA256` hashes for bytes streams             | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHashing.h)                 
[Http](https://pagghiu.github.io/SaneCppLibraries/library_http.html)                                            | 🟥 HTTP parser, client and server                                         | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHttp.h)                    
[Memory](https://pagghiu.github.io/SaneCppLibraries/library_memory.html)                                        | 🟩 Custom allocators, Virtual Memory, Buffer, Segment                     | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppMemory.h)                  
[Plugin](https://pagghiu.github.io/SaneCppLibraries/library_plugin.html)                                        | 🟨 Minimal dependency based plugin system with hot-reload                 | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppPlugin.h)                  
[Process](https://pagghiu.github.io/SaneCppLibraries/library_process.html)                                      | 🟩 Create child processes and redirect their input / output               | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppProcess.h)                 
[Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html)                                | 🟩 Describe C++ types at compile time for serialization                   | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppReflection.h)              
[Serialization Binary](https://pagghiu.github.io/SaneCppLibraries/library_serialization_binary.html)            | 🟨 Serialize to and from a binary format using Reflection                 | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSerializationBinary.h)     
[Serialization Text](https://pagghiu.github.io/SaneCppLibraries/library_serialization_text.html)                | 🟨 Serialize to / from text formats (JSON) using Reflection               | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSerializationText.h)       
[Socket](https://pagghiu.github.io/SaneCppLibraries/library_socket.html)                                        | 🟨 Synchronous socket networking and DNS lookup                           | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSocket.h)                  
[Strings](https://pagghiu.github.io/SaneCppLibraries/library_strings.html)                                      | 🟩 String formatting / conversion / manipulation (UTF8 / UTF16)           | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppStrings.h)                 
[Testing](https://pagghiu.github.io/SaneCppLibraries/library_testing.html)                                      | 🟨 Simple testing framework used by all of the other libraries            | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppTesting.h)                 
[Threading](https://pagghiu.github.io/SaneCppLibraries/library_threading.html)                                  | 🟩 Atomic, thread, mutex, semaphore, barrier, rw-lock, condition          | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppThreading.h)               
[Time](https://pagghiu.github.io/SaneCppLibraries/library_time.html)                                            | 🟨 Time handling (relative, absolute, high resolution)                    | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppTime.h)                    

Each library is color-coded to signal its status:  
🟥 Draft (incomplete, WIP, works on basic test cases)  
🟨 MVP (a minimum set of features has been implemented)  
🟩 Usable (a reasonable set of useful features has been implemented)  

<picture>
  <img alt="Sane C++ Libraries dependencies" src="https://pagghiu.github.io/images/dependencies/SaneCppLibrariesDependencies.svg">
</picture>

## How to use Sane C++ Libraries in your project

### Option 1: use single file libraries
1. Obtain a specific library: 
    - Downloading it from the above table or from the [Latest Release](https://github.com/Pagghiu/SaneCppLibraries/releases/latest) page 
    - Assembling it from current `main` branch using the [Single File Library](https://pagghiu.github.io/SaneCppLibraries/page_single_file_libs.html) browser app
2. `#define SC_COMPILER_ENABLE_STD_CPP=1` if you plan to use the Standard C++ library
3. `#include SaneCppLIBRARY.h` in your headers
4. `#define SANE_CPP_IMPLEMENTATION` + `#include "SaneCppLIBRARY.h"` in one of your `.cpp` files

See [Building (user)](https://pagghiu.github.io/SaneCppLibraries/page_building_user.html) for details on the (system) libraries to link.  

### Option 2: use all libraries together
1. Clone the entire repo and add it as subfolder of your project
2. Add [SC.cpp](SC.cpp) to your build system of choice
3. `#define SC_COMPILER_ENABLE_STD_CPP=1` if you plan to use the Standard C++ library
4. Include any public header (`Libraries/[Library]/*.h`)

See [Building (user)](https://pagghiu.github.io/SaneCppLibraries/page_building_user.html) for details on the (system) libraries to link.

## Examples

- [SCTest Suite](Tests/Libraries) showcases most functionalities of all libraries.
  Check [Building (contributor)](https://pagghiu.github.io/SaneCppLibraries/page_building_contributor.html) for details.
- [Documentation](https://pagghiu.github.io/SaneCppLibraries/libraries.html) page of each library embeds some examples and / or code snippets.
- [Examples](https://pagghiu.github.io/SaneCppLibraries/page_examples.html) showcases some basic examples like an `AsyncWebServer` and a more advanced `SCExample` an [Async](https://pagghiu.github.io/SaneCppLibraries/library_async.html) event loop integration with a GUI and usage of [Plugin](https://pagghiu.github.io/SaneCppLibraries/library_plugin.html).
- [Tools](https://pagghiu.github.io/SaneCppLibraries/page_tools.html) is a collection of repository / code automation tools built using libraries themselves
  - Includes a fully self-hosted [SC::Build](https://pagghiu.github.io/SaneCppLibraries/page_build.html) build system (generator) where builds are imperatively described using C++ code.

## No Allocations

- All libraries do not dynamically allocate memory (excluding [Memory](https://pagghiu.github.io/SaneCppLibraries/library_memory.html) and [Containers](https://pagghiu.github.io/SaneCppLibraries/library_containers.html))
- All libraries are designed to work inside user-provided memory buffers.
- All libraries return error codes when running out of such memory buffers.
- Third-party container classes, including `std::` ones, are supported (see [InteropSTL](Tests/InteropSTL) for an example).
- [Memory](https://pagghiu.github.io/SaneCppLibraries/library_memory.html) and [Containers](https://pagghiu.github.io/SaneCppLibraries/library_containers.html) are fully optional and just provided for convenience if user prefers them to `std::` or other equivalent containers library.
- [Memory](https://pagghiu.github.io/SaneCppLibraries/library_memory.html) and [Containers](https://pagghiu.github.io/SaneCppLibraries/library_containers.html) are not used by any other library (excluding [Containers Reflection](https://pagghiu.github.io/SaneCppLibraries/library_containers_reflection.html) a small bridge library to describe [Containers](https://pagghiu.github.io/SaneCppLibraries/library_containers.html) to [Reflection](https://pagghiu.github.io/SaneCppLibraries/library_reflection.html) library for easy serialization).

## Documentation
[Documentation](https://pagghiu.github.io/SaneCppLibraries/index.html) is automatically generated using Doxygen and updated at every commit to `main` branch.

## Getting in touch

- [Sane Coding Discord](https://discord.gg/tyBfFp33Z6)  
- [X](https://x.com/pagghiu_) `@pagghiu_`
- [Github Discussion](https://github.com/Pagghiu/SaneCppLibraries/discussions)

## Contributing

- [Principles](https://pagghiu.github.io/SaneCppLibraries/page_principles.html) 
- [Coding Style](https://pagghiu.github.io/SaneCppLibraries/page_coding_style.html)
- [CONTRIBUTING.md](CONTRIBUTING.md)

## License

Sane C++ Libraries are licensed under the MIT License, see [LICENSE.txt](LICENSE.txt) for more information.

## Videos

On this [YouTube Channel](https://www.youtube.com/@Pagghiu) there are some videos showing bits of the development process.

## Blog posts

On [Sane Coding Blog](https://pagghiu.github.io) there is a series of posts about this project.

Relevant yearly posts:

- [Blog Post - Sane C++ Libraries (Open Sourcing)](https://pagghiu.github.io/site/blog/2023-12-23-SaneCppLibrariesRelease.html)
- [Blog Post - 1st Year of Sane C++ Libraries](https://pagghiu.github.io/site/blog/2024-12-23-SaneCpp1Year.html)
- [Blog Post - 2nd year of Sane C++ Libraries](https://pagghiu.github.io/site/blog/2025-12-23-SaneCpp2Year.html)

## External
- [DeepWiki/SaneCppLibraries](https://deepwiki.com/Pagghiu/SaneCppLibraries) (AI-guided walkthrough of the project)
