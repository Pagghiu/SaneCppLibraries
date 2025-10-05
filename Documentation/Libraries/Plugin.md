@page library_plugin Plugin

@brief 🟨 Minimal dependency based plugin system with hot-reload

[TOC]

[SaneCppPlugin.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppPlugin.h) is a library that allows extending application compiling C++ source to executable code at runtime.

# Quick Sheet

\snippet Tests/Libraries/Plugin/PluginTest.cpp PluginSnippet

@note The above code is simplified. For a more complete implementation of an hot-reload system see [SCExample](@ref page_examples) code (`HotReloadSystem.h`).

# Dependencies
- Dependencies: [FileSystemIterator](@ref library_file_system_iterator), [Process](@ref library_process), [Strings](@ref library_strings), [Time](@ref library_time)
- All dependencies: [File](@ref library_file), [FileSystemIterator](@ref library_file_system_iterator), [Foundation](@ref library_foundation), [Process](@ref library_process), [Strings](@ref library_strings), [Time](@ref library_time)

![Dependency Graph](Plugin.svg)


# Features
- Compile and link cpp files to dynamic library
- Unload / recompile / reload Plugin
- Reload all dependant plugins of a given one
- PluginDynamicLibrary::queryInterface allows creating contracts between Plugins or Plugins and Host
- Support creating libc++ and libc free plugins that only use Sane C++ Libraries
- Allows toolchain customization through SC::PluginSysroot and SC::PluginCompiler
- Forcefully unlock and delete dll being debugged from Visual Studio Debugger

# Status
🟨 MVP  
This library is expected to work correctly on `macOS`, `Windows`, and `Linux` using `MSVC`, `clang` and `GCC` compiler toolchains.

# Description
The main use case is for now splitting applications in smaller pieces that can be hot-reloaded at runtime.  
A secondary use case could be allowing customization on a delivered application (mainly on Desktop systems).
Plugins are always meant to be delivered in source code form (`.cpp`) and they're compiled on the fly.
A plugin is made of a single `.cpp` file and it declares itself through a special comment in the source code.
Such comment can declare the name, the version, a description / category and a list of dependencies.  

A plugin can be modified, unloaded, re-compiled and re-loaded to provide additional functionality.

The list of dependencies makes it possible to find recursive dependencies and unload them before unload a plugin.

The library doesn't use a build system, but it compiles the `.cpp` files directly, linking it with symbols exported from
the loading executable (using `bundle_loader` on macOS and linking library exported from loading executable on windows).
Plugin Dynamic Libraries are compiled with `nostdlib` and `nostdlib++` and they include a stub the allows defining some symbols needed due to not linking the C++ CRT.  
Some special build flags however allow using `libc`, `libc++` or other sysroot / compiler supplied windows.  
The idea is that plugins only use functionality provided by the calling executable or by other plugins.

On Windows, some extra care has been taken to force-unlock the `.pdb` file from visual studio debugger, that happens if the dll is being loaded on a program being debugged.

As of today this is all implemented using native dynamic library mechanisms that are being loaded directly in the process.  
Doing the wrong thing with memory or forgetting to clean everything during shutdown can quickly crash the main executable.

# Videos

This is the list of videos that have been recorded showing some usages of the library:

- [Ep.22 - Hot-Reload dear imgui](https://www.youtube.com/watch?v=BXybEWvSpGU)
- [Ep.24 - Hot-Reload C++ on iOS](https://www.youtube.com/watch?v=6DfykfYCQdY)
- [Ep.25 - C++ Serialization and Reflection (with Hot-Reload)](https://www.youtube.com/watch?v=d7DXxC6xG_A)

# Blog

Some relevant blog posts are:

- [June 2024 Update](https://pagghiu.github.io/site/blog/2024-06-30-SaneCppLibrariesUpdate.html)
- [July 2024 Update](https://pagghiu.github.io/site/blog/2024-07-31-SaneCppLibrariesUpdate.html)

# Examples

- [SCExample](@ref page_examples) uses `Plugin` library for a simple hot-reload system
- Unit test inside `PluginTest.cpp` show how the API is meant to be used

# Roadmap

There are plans to experiment with out of process plugins using some sort of RPC system (like Audio Unit plugins on macOS)
and / or experimenting using WASM as a plugin host to eliminate such instability / security issues.
Other ideas include redistribute a minimal C++ toolchain (probably a customized clang) that can compile
the plugins without needing a system compiler or a sysroot, as all public headers of libraries in this project do not need
any system or compiler header.

🟩 Usable Features:
- Specify directory for compiled intermediate and output files
- Parallel / Async compile and link
- Improve error handling and reporting
- Further customization of some build flags and features:
    - Custom libraries to link (declared in the plugin)
    - Custom include paths (declared in the plugin)

🟦 Complete Features:
- Create minimal clang toolchain to compile scripts on non-developer machines
- Integrate with [Build](@ref page_build) library (once it will gain capability to build standalone without needing Xcode or Visual Studio)
- Evaluate possibility to achieve some minimal error recovery
- Easily integration with some RPC mechanism

💡 Unplanned Features:
- Compile plugins to WASM ?
- Deploy closed-source (already compiled) binary plugins
- Allow plugin to be compiled with different compiler from the one used in the Host

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 363			| 230		| 593	|
| Sources   | 1246			| 218		| 1464	|
| Sum       | 1609			| 448		| 2057	|
