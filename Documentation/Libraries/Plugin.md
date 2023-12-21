@page library_plugin Plugin

@brief 🟥 Minimal dependency based plugin system with hot-reload

[TOC]

Plugin library allows extending application compiling C++ source to executable code at runtime.

# Features
- Compile and link cpp files to dynamic library
- Unload / recompile / reload Plugin
- Reload all dependant plugins of a given one
- Forcefully unlock and delete dll being debugged from Visual Studio Debugger

# Status
🟥 Draft  
This entire system is still in early stages, and it's an experiment at best,so it's not recommended for general use.

# Description
The main use case is for now splitting applications in smaller pieces that can be hot-reloaded at runtime.  
A secondary use case could be allowing customization on a delivered application (mainly on Desktop systems).
Plugins are always meant to be delivered in source code form (.cpp) and they're compiled on the fly.
A plugin is made of a single .cpp file and it declares itself through a special comment in the source code.
Such comment can declare the name, the version, a description / category and a list of dependencies.  

A plugin can be modified, unloaded, re-compiled and re-loaded to provide additional functionality.

The list of dependencies makes it possible to find recursive dependencies and unload them before unload a plugin.

The library doesn't use a build system, but it compiles the .cpp files directly, linking it with symbols exported from
the loading executable (using `bundle_loader` on macOS and linking library exported from loading executable on windows).
Plugin Dynamic Libraries are compiled with `nostdlib` and `nostdlib++` and they include a stub the allows defining some 
symbols needed due to not linking the C++ CRT.  
The idea is that plugins only use functionality provided by the calling executable or by other plugins.

On Windows, some extra care has been taken to force-unlock the .pdb file from visual studio debugger, that happens
if the dll is being loaded on a program being debugged.

As of today this is all implemented using native dynamic library mechanisms and they're loaded
in process, so doing the wrong thing with memory or forgetting to clean everything during shutdown can yield to instabilities
and eventually crash the main executable.

# Examples

No examples are provided so far as the API is very likely to change drastically going towards MVP.  
If you like to see where we are just a take a look at the associated unit test.

# Roadmap

There are plans to experiment with out of process plugins using some sort of RPC system (like Audio Unit plugins on macOS)
and / or experimenting using WASM as a plugin host to eliminate such instability / security issues.
Other ideas include redistribute a minimal C++ toolchain (probably a customized clang) that can compile
the plugins without needing a system compiler or a sysroot, as all public headers of libraries in this project do not need
any system or compiler header.

🟨 MVP
- Implement the API on Linux
- Integrate with [FileSystemWatcher](@ref library_file_system_watcher) to get hot-reload during development.

🟩 Usable Features:
- Evaluate integration with [Build](@ref library_build) once it will gain capability to build standalone (without XCode or Visual Studio)
- Create minimal clang toolchain to compile scripts on non-developer machines

🟦 Complete Features:
- To be decided

💡 Unplanned Features:
- Compile plugins to WASM ?
