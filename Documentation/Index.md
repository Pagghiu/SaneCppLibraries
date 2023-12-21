# Sane C++

[TOC]
**Sane C++** is a set of C++ platform abstraction libraries for macOS and Windows ([Platforms](#autotoc_md3)).  

Project [Principles](@ref page_principles):

@copybrief page_principles

## Motivation

- Having fun building from scratch a cohesive ecosystem of libraries sharing the same core principles
- Fight bloat measured in cognitive and build complexity, compile time, binary size and debug performance
- Providing out-of-the-box functionalities typically given for granted in every respectable modern language
- [Re-invent the wheel](https://xkcd.com/927/) hoping it will be more round this time
- You can take a look at the [introductory blog post](https://pagghiu.github.io/site/blog/2023-12-23-SaneCppLibrariesRelease.html) if you like

## Status
Many libraries are in draft state, while others are slightly more usable.  
Click on specific page each library to know about its status.  

- 🟥 Draft (incomplete, work in progress, proof of concept, works on basic case)
- 🟨 MVP (minimum set of features have been implemented)
- 🟩 Usable (a reasonable set of features has been implemented to make library useful)
- 🟦 Complete (all planned features have been implemented)

It is a deliberate decision to prototype single libraries and make them public Draft or MVP state.  
This is done so that they can be matured in parallel with all other libraries and evolve their API more naturally.  

@copydetails libraries

## Platforms

Supported:
- macOS
- Windows

Planned:
- iOS
- Linux
- WASM (Emscripten / WASI)

Not Planned:
- Android

## Learning

One way to learn and explore the library is to read and / or step through the extensive set of unit tests (current test code coverage is > 90%).

## Building

### Dependencies

None (aside from your C++ compiler and its SDK / Sysroot)

### Integrate in your project
- Add:
    - `SC.cpp` (located at `Bindings/cpp`)
- Link (**macOS**):
    -  `CoreFoundation.framework`
    -  `CoreServices.framework`
- Link (**Windows**):
    - Nothing (implicitly linking `Ws2_32.lib`, `ntdll.lib`, `Rstrtmgr.lib` and `shlwapi.lib` through `#pragma comment(lib, ...)`)
- Include:
    - Include public headers `Libraries/**SomeLibrary**/SomeHeader.h`

@warning All files in *Internal* or *Tests* sub-folders of each library are considered private / implementation details.  
Only the headers in the root directory of a library are considered public.

@note If you're using the Standard C++ Library you should define also define the following preprocessor macro:  
`SC_COMPILER_ENABLE_STD_CPP=1`.  
You can also choose to [Disable the standard C++ library](@ref page_how_to) in your project if you're satisfied with what's provided by the libraries and you will not need to define the macro at all.

### Build the test

- Generate test project (run `SCBuild.sh` or `SCBuild.bat`)
- Open generated projects (in `_Build/Projects`). 

Projects are generated using the self-hosted [Build](@ref library_build) system that allows describing projects using C++.  
The library is compiled with a single `clang` (or `cl.exe`) invocation and the resulting executable generates projects.

### ABI / API Stability

There are no plans to provide ABI stability.

Each library declares its own API stability, but as the project is very young, expect breaking changes everywhere / every time for now.  
At some point API will stabilize naturally and it will be made explicit for each library.

