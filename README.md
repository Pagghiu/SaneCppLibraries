[![Windows](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/windows.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/windows.yml)
[![Linux+macOS](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/posix.yml/badge.svg)](https://github.com/Pagghiu/SaneCppLibraries/actions/workflows/posix.yml)
[![Coverage](https://pagghiu.github.io/SaneCppLibraries/reference/doxygen/coverage/coverage.svg)](https://pagghiu.github.io/SaneCppLibraries/reference/doxygen/coverage/)

# Sane C++ Libraries

[![YouTube](https://img.shields.io/youtube/channel/subscribers/UCnmN_whfM12LU6VNQWG0NFg)](https://youtube.com/@Pagghiu)
[![X](https://img.shields.io/twitter/follow/pagghiu_)](https://x.com/pagghiu_)
[![Discord](https://img.shields.io/discord/1195076118307426384)](https://discord.gg/tyBfFp33Z6)
![GitHub Repo stars](https://img.shields.io/github/stars/Pagghiu/SaneCppLibraries)

**Sane C++ Libraries** is a set of small C++ platform abstraction libraries for macOS, Windows, and Linux.

The project is designed around explicit APIs, low dependency pressure, single-file integration, and code that can be safely maintained with coding agents.

## Quick links

- [Website](https://pagghiu.github.io/SaneCppLibraries/)
- [Library catalog](https://pagghiu.github.io/SaneCppLibraries/libraries/)
- [Guides](https://pagghiu.github.io/SaneCppLibraries/guides/)
- [Reference documentation](https://pagghiu.github.io/SaneCppLibraries/reference/doxygen/)
- [Contributing](CONTRIBUTING.md)
- [Blog](https://pagghiu.github.io)

## Principles

The full project principles are documented here:
[Principles](https://pagghiu.github.io/SaneCppLibraries/guides/principles/).

In short:

- Fast compile times
- Bloat-free code
- Simple and readable implementation
- Easy integration
- Minimal dependencies between libraries
- No hidden allocations in normal library code
- No third-party build dependencies when an operating-system API can be wrapped directly

## Agentic development model

Sane C++ Libraries is now maintained primarily through agentic development.

For contributors, this means a useful issue can be the whole contribution. The preferred workflow is to describe the desired change, the affected library or platform, the constraints, and the expected validation. A reusable prompt is welcome when it helps the maintainer replay or adapt the work.

Code pull requests are not the preferred path. Human-written code-only pull requests are not accepted, and agent-written pull requests need a tested reproduction prompt plus human sanity-checking.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the current workflow.

## Using the libraries

### Option 1: single-file libraries

Use this path when you want to vendor one library without adopting the whole repository.

1. Download a release header from the table below or from the [latest release](https://github.com/Pagghiu/SaneCppLibraries/releases/latest).
2. Include `SaneCppLIBRARY.h` from your headers.
3. In one `.cpp` file, define `SANE_CPP_IMPLEMENTATION` before including the same header.

You can also generate single-file headers from the current branch using the [Single File Library browser](https://pagghiu.github.io/SaneCppLibraries/reference/doxygen/page_single_file_libs.html).

### Option 2: repository checkout

Use this path when you want several libraries together or want to keep the source tree available.

1. Clone the repository and add it as a subfolder of your project.
2. Add [SC.cpp](SC.cpp) to your build system.
3. Include the headers for the libraries you use.

See [Building (user)](https://pagghiu.github.io/SaneCppLibraries/guides/building-user/) for platform-specific system libraries and linker details.

### Option 3: SC::Build

Use the repository build tooling when you want examples, tests, native builds, cross targets, or package-managed toolchains.

- [SC::Build guide](https://pagghiu.github.io/SaneCppLibraries/guides/build/)
- [External SC::Build bootstrap](https://pagghiu.github.io/SaneCppLibraries/guides/build-external/)
- [Contributor build guide](https://pagghiu.github.io/SaneCppLibraries/guides/building-contributor/)

## Libraries

Libraries are designed to stay independent and work well as [single-file libraries](https://pagghiu.github.io/SaneCppLibraries/reference/doxygen/page_single_file_libs.html). The [dependency graph](https://pagghiu.github.io/SaneCppLibraries/reference/doxygen/page_dependencies.html) is part of the public project documentation.

Status legend:

- 🟥 Draft: incomplete, work in progress, works on basic test cases
- 🟨 MVP: a minimum useful feature set has been implemented
- 🟩 Usable: a reasonable set of useful features has been implemented

Library | Description | Single File
:--|:--|:--
[Async](https://pagghiu.github.io/SaneCppLibraries/libraries/async/) | 🟨 Async I/O for files, sockets, timers, processes, filesystem events, and tasks | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAsync.h)
[Async Streams](https://pagghiu.github.io/SaneCppLibraries/libraries/async-streams/) | 🟨 Concurrently read, write, and transform byte streams | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAsyncStreams.h)
[Await](https://pagghiu.github.io/SaneCppLibraries/libraries/await/) | 🟨 C++20 coroutine layer over Async | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAwait.h)
[Containers](https://pagghiu.github.io/SaneCppLibraries/libraries/containers/) | 🟨 Generic containers such as `SC::Vector`, `SC::SmallVector`, and `SC::Array` | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppContainers.h)
[Containers Reflection](https://pagghiu.github.io/SaneCppLibraries/libraries/containers-reflection/) | 🟨 Container specializations for Reflection and Serialization | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppContainersReflection.h)
[File](https://pagghiu.github.io/SaneCppLibraries/libraries/file/) | 🟩 Synchronous disk file I/O | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFile.h)
[File System](https://pagghiu.github.io/SaneCppLibraries/libraries/file-system/) | 🟩 File and directory operations such as copy, remove, and rename | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystem.h)
[File System Iterator](https://pagghiu.github.io/SaneCppLibraries/libraries/file-system-iterator/) | 🟩 Enumerates files and directories inside a path | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystemIterator.h)
[File System Watcher](https://pagghiu.github.io/SaneCppLibraries/libraries/file-system-watcher/) | 🟩 Filesystem notifications for add, remove, rename, and modify events | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystemWatcher.h)
[Foundation](https://pagghiu.github.io/SaneCppLibraries/libraries/foundation/) | 🟩 Primitive types, asserts, compiler macros, `Function`, `Span`, and `Result` | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFoundation.h)
[Hashing](https://pagghiu.github.io/SaneCppLibraries/libraries/hashing/) | 🟩 Computes `MD5`, `SHA1`, and `SHA256` hashes for byte streams | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHashing.h)
[Http](https://pagghiu.github.io/SaneCppLibraries/libraries/http/) | 🟥 HTTP parser, client, server, and WebSocket support | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHttp.h)
[Http Client](https://pagghiu.github.io/SaneCppLibraries/libraries/http-client/) | 🟥 Streaming-first HTTP client with native OS backends | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHttpClient.h)
[Memory](https://pagghiu.github.io/SaneCppLibraries/libraries/memory/) | 🟩 Custom allocators, virtual memory, buffers, and segments | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppMemory.h)
[Plugin](https://pagghiu.github.io/SaneCppLibraries/libraries/plugin/) | 🟨 Minimal dependency-based plugin system with hot reload | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppPlugin.h)
[Process](https://pagghiu.github.io/SaneCppLibraries/libraries/process/) | 🟩 Creates child processes and redirects their input and output | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppProcess.h)
[Reflection](https://pagghiu.github.io/SaneCppLibraries/libraries/reflection/) | 🟩 Describes C++ types at compile time for serialization | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppReflection.h)
[SerialPort](https://pagghiu.github.io/SaneCppLibraries/libraries/serial-port/) | 🟨 Serial port configuration and handling | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSerialPort.h)
[Serialization Binary](https://pagghiu.github.io/SaneCppLibraries/libraries/serialization-binary/) | 🟨 Binary serialization using Reflection | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSerializationBinary.h)
[Serialization Text](https://pagghiu.github.io/SaneCppLibraries/libraries/serialization-text/) | 🟨 Text serialization using Reflection | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSerializationText.h)
[Socket](https://pagghiu.github.io/SaneCppLibraries/libraries/socket/) | 🟨 Synchronous socket networking and DNS lookup | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSocket.h)
[Strings](https://pagghiu.github.io/SaneCppLibraries/libraries/strings/) | 🟩 String formatting, conversion, and manipulation for UTF-8 and UTF-16 | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppStrings.h)
[Testing](https://pagghiu.github.io/SaneCppLibraries/libraries/testing/) | 🟨 Simple testing framework used by the other libraries | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppTesting.h)
[Threading](https://pagghiu.github.io/SaneCppLibraries/libraries/threading/) | 🟩 Atomics, threads, mutexes, semaphores, barriers, rw-locks, and conditions | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppThreading.h)
[Time](https://pagghiu.github.io/SaneCppLibraries/libraries/time/) | 🟨 Relative, absolute, and high-resolution time handling | [Download](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppTime.h)

<picture>
  <img alt="Sane C++ Libraries dependencies" src="https://pagghiu.github.io/images/dependencies/SaneCppLibrariesDependencies.svg">
</picture>

## Constraints

These constraints are part of the project identity and are especially important when code is generated or regenerated by agents:

- Keep library boundaries explicit.
- Avoid accidental internal dependencies.
- Prefer caller-owned memory and explicit error handling.
- Avoid hidden dynamic allocations in normal library code.
- Keep platform-specific code isolated.
- Add focused tests for new behavior when practical.

Allocation notes:

- Most libraries are designed to work inside user-provided memory buffers.
- APIs report allocation failures explicitly, usually through `SC::Result`.
- [Memory](https://pagghiu.github.io/SaneCppLibraries/libraries/memory/) and [Containers](https://pagghiu.github.io/SaneCppLibraries/libraries/containers/) allocate by default, but can also work with fixed memory buffers.
- [Await](https://pagghiu.github.io/SaneCppLibraries/libraries/await/) uses caller-owned fixed buffers by default; virtual memory, `malloc` / `free`, and polymorphic allocators are explicit opt-in modes.
- Third-party containers, including `std::` containers, are supported. See [InteropSTL](Tests/InteropSTL) for an example.

## When using coding agents

`README.md` is written for humans. If you ask an agent to work on this repository, point it at the project instructions first:

1. Read [AGENTS.md](AGENTS.md).
2. Read [CONTRIBUTING.md](CONTRIBUTING.md).
3. Read the relevant library page and local `AGENTS.md` files before editing code.
4. Name the affected library, tool, or platform.
5. Preserve the constraints above.
6. Run focused validation and report what was not tested.

## Documentation

- [Website](https://pagghiu.github.io/SaneCppLibraries/)
- [Guides](https://pagghiu.github.io/SaneCppLibraries/guides/)
- [Library catalog](https://pagghiu.github.io/SaneCppLibraries/libraries/)
- [Reference documentation](https://pagghiu.github.io/SaneCppLibraries/reference/doxygen/)
- [Coverage](https://pagghiu.github.io/SaneCppLibraries/reference/doxygen/coverage/)
- [DeepWiki/SaneCppLibraries](https://deepwiki.com/Pagghiu/SaneCppLibraries)

## Examples

- [SCTest Suite](Tests/Libraries) covers most library functionality.
- [Examples](https://pagghiu.github.io/SaneCppLibraries/guides/examples/) includes examples such as `AsyncWebServer` and the more advanced `SCExample`.
- [Tools](https://pagghiu.github.io/SaneCppLibraries/guides/tools/) documents repository and code automation tools built with the libraries themselves.

## Contributing

Contributions are issue-first and agent-friendly.

Start here:

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [Principles](https://pagghiu.github.io/SaneCppLibraries/guides/principles/)
- [Coding Style](https://pagghiu.github.io/SaneCppLibraries/guides/coding-style/)

## Community

- [Sane Coding Discord](https://discord.gg/tyBfFp33Z6)
- [X](https://x.com/pagghiu_) `@pagghiu_`
- [GitHub Discussions](https://github.com/Pagghiu/SaneCppLibraries/discussions)
- [YouTube](https://www.youtube.com/@Pagghiu)

## Blog posts

On the [Sane Coding Blog](https://pagghiu.github.io) there is a series of posts about this project.

Relevant yearly posts:

- [Sane C++ Libraries: open sourcing](https://pagghiu.github.io/site/blog/2023-12-23-SaneCppLibrariesRelease.html)
- [1st year of Sane C++ Libraries](https://pagghiu.github.io/site/blog/2024-12-23-SaneCpp1Year.html)
- [2nd year of Sane C++ Libraries](https://pagghiu.github.io/site/blog/2025-12-23-SaneCpp2Year.html)

## License

Sane C++ Libraries are licensed under the MIT License. See [LICENSE.txt](LICENSE.txt) for more information.
