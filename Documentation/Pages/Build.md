@page page_build SC::Build

`SC::Build` is a [Tool](@ref page_tools) and public API for describing builds in C++ and then either generating project files or building directly through a standalone native backend.

[TOC]

# Features

- Describe builds in `SC-build.cpp` using `SC::Build::Definition`, `Workspace`, `Project`, and `Configuration`
- Generate XCode 14+ projects, Visual Studio 2019 / 2022 projects, and Makefiles
- Build directly through `SC::Build::Generator::Native` on macOS, Linux, and Windows hosts
- Build console executables, GUI applications, shared libraries, and static libraries
- Attach compile/link settings at project, configuration, and per-file granularity
- Build workspace-local static-library dependencies in dependency order on the native backend
- Track header dependencies through compiler-generated dependency information
- Emit `compile_commands.json` on the native backend and from the Make / XCode generated flows
- Integrate with `SC-package` from [Tools](@ref page_tools) to download repository dependencies
- Export Sane C++ libraries for [Plugin](@ref library_plugin) hosts through `addExportLibraries` and `addExportAllLibraries`

# Status

🟨 MVP

`SC::Build` is used by this repository to generate projects and/or build the test suites, tools, examples, and the `SC`
shared library. The generated-project backends remain part of the daily workflow, while the standalone native backend now
supports direct host builds on macOS, Linux, and Windows.

# Description

Build descriptions are regular C++ files, conventionally named `SC-build.cpp`, that are compiled on the fly through the
bootstrap scripts `SC.sh` and `SC.bat`.

The same public API serves two workflows:

- Generated backends: emit XCode, Visual Studio, or Make projects into `_Build/_Projects`
- Native backend: invoke compiler, linker, and archiver child processes directly without generating project files first

The repository tool surface is the `SC-build` tool described in [Tools](@ref page_tools). Its command-line shape is:

```text
./SC.sh build configure [workspace:project | project]
./SC.sh build compile [workspace:project | project] [configuration] [generator] [architecture] [-- extra args...]
./SC.sh build run [workspace:project | project] [configuration] [generator] [architecture] [-- extra args...]
./SC.sh build coverage [workspace:project | project] [configuration] [generator] [architecture]
```

Generator keywords accepted by the tool are:

- `default`
- `native`
- `make`
- `xcode`
- `vs2022`
- `vs2019`

Architecture keywords accepted by the tool are:

- `arm64`
- `intel64`
- `intel32`
- `wasm`
- `any`

Current CLI behavior:

- If no project is specified, `compile` builds the whole default workspace
- `run` requires a single executable target and forwards arguments placed after `--`
- If no configuration is specified, `Debug` is used
- Host-default generator selection is `vs2022` on Windows and `make` on macOS / Linux
- `configure` is primarily for generated backends; the native backend builds directly into `_Build/_Outputs` and `_Build/_Intermediates`

This is the repository `Tools/SC-build.cpp` file used to configure the default workspace:
\include Tools/SC-build.cpp

# Public API

The public API in `Tools/SC-build/Build.h` currently exposes:

- `Definition`, `Workspace`, `Project`, and `Configuration` for the build graph
- `TargetType::{ConsoleExecutable, GUIApplication, SharedLibrary, StaticLibrary}`
- `Generator::{Native, XCode, VisualStudio2022, VisualStudio2019, Make}`
- `Toolchain::{HostDefault, Clang, GCC, MSVC, ClangCL, ZigCC, CustomDriver}`
- `ExecutionOptions` for native-backend parallelism / verbosity knobs
- `Project::addSpecificFileFlags` for per-file flag groups
- `Project::addExportLibraries`, `addExportAllLibraries`, and `addExportDirectories` for plugin host exports

# Backend Matrix

| Backend | Platforms | Target kinds | `compile_commands.json` | Notes |
|:--|:--|:--|:--|:--|
| `Native` | macOS, Linux, Windows hosts | Console executable, GUI application, shared library, static library | Yes | Direct compile / run / coverage flow; no project files are generated |
| `XCode` | Apple | Console executable, GUI application, shared library, static library | Yes | GUI apps also emit storyboard / assets; `Intel32` and `Wasm` architectures are unsupported |
| `VisualStudio2022` / `VisualStudio2019` | Windows | Console executable, GUI application, shared library, static library | No | The repository defaults to VS2022, but VS2019 generation is still available |
| `Make` | Apple, Linux | Console executable, GUI application, shared library, static library | Yes on clang-style flows | GCC builds still compile, but Makefile-side compile database generation is unavailable there |

# Native Backend

The standalone backend is selected through `SC::Build::Generator::Native`.

Current implemented scope:

- Host platforms: macOS, Linux and Windows
- Toolchain families exposed by the API: `HostDefault`, `Clang`, `GCC`, `MSVC`, `ClangCL`, `ZigCC`, and `CustomDriver`
- Target kinds: console executables, GUI applications, shared libraries, and static libraries
- Dependency tracking: compiler-generated dependency files / dependency output
- Incrementality: skips up-to-date compile and link steps
- Workspace dependency ordering: native executable targets can link workspace-local static libraries by name
- Parallelism: compile fan-out is controlled by `ExecutionOptions::maxParallelJobs`
- Output layout: `_Build/_Outputs`, `_Build/_Intermediates`, and `_Build/_BuildCache`

Typical native commands:

```bash
./SC.sh build compile SCBuildTest Debug native
./SC.sh build run SCBuildTest Debug native -- --test "BuildTest"
SC.bat build compile SCTest Debug native
```

Important current limits:

- Windows native sysroot selection is not implemented yet
- `run` is valid only for executable targets and only when a single project is selected
- The repository `build configure` command does not rely on a Windows-native generation pass because native builds do
  not need generated project files

# Per-file Flags

`Project::addSpecificFileFlags` is implemented and used by this repository.

The current backend behavior is:

| Backend | Per-file behavior |
|:--|:--|
| `Native` | Full compile-flag merge for the selected files |
| `Make` | Full compile-flag merge, emitted as grouped per-file variables in the generated Makefile |
| `XCode` | Per-file include paths, defines, and non-MSVC warning disables are emitted; other per-file compile knobs are not yet emitted, and conflicting higher-level flags cannot be removed |
| `VisualStudio2019` / `VisualStudio2022` | Per-file include paths, defines, and MSVC warning disables are emitted; other per-file compile knobs are not yet emitted |

The repository example lives in `Tools/SC-build.cpp`, where the `SCTest` target applies a dedicated define, include path,
and warning disables to a selected subset of files.

# Plugin Host Exports

For host executables using the [Plugin](@ref library_plugin) library:

- `Project::addExportLibraries` adds the needed `SC_EXPORT_LIBRARY_<LIBRARY>=1` defines to the host executable
- `Project::addExportAllLibraries` does the same for every Sane C++ library
- On Linux both helpers also add `-rdynamic`

`LinkFlags::preserveExportedSymbols` currently performs fine-grained exported-symbol preservation only on the standalone
native backend and on generated Makefiles. The XCode backend currently falls back to disabling dead-code stripping when
preservation is requested.

# Generated Layout

Repository-generated paths are currently:

- `_Build/_Projects/XCode/SCWorkspace/...`
- `_Build/_Projects/VisualStudio2022/SCWorkspace/...`
- `_Build/_Projects/VisualStudio2019/SCWorkspace/...`
- `_Build/_Projects/Make/SCWorkspace/apple/...`
- `_Build/_Projects/Make/SCWorkspace/linux/...`
- `_Build/_Outputs/<expanded-configuration>/...`
- `_Build/_Intermediates/<project>/<expanded-configuration>/...`
- `_Build/_Intermediates/<workspace>/compile_commands.json` for native workspace databases
- `_Build/_BuildCache/...` for native-backend state

# How To Debug

1. Build `SC-build` itself through one of the repository build tasks or through `build compile`
2. Launch the generated `SC-build` executable under your debugger with the same arguments passed by the bootstrap

With any debugger or IDE other than VSCode, debug `_Build/_Tools/<Platform>/SC-build` (or
`_Build/_Tools/Windows/SC-build.exe`) and pass arguments similar to:

```json
"name": "Debug SC-build [linux] (lldb)",
"type": "lldb",
"request": "launch",
"program": "${workspaceFolder}/_Build/_Tools/Linux/SC-build",
"args": [
    "${workspaceFolder}",
    "${workspaceFolder}/Tools",
    "${workspaceFolder}/_Build",
    "build",
    "configure",
    "SCTest",
    "Debug"
],
```

# Videos

This is the list of videos that have been recorded showing some of the internal thoughts that have been going into this library:

- [Ep.02 - Creating a Makefile](https://www.youtube.com/watch?v=2ccW8TBAWWE)
- [Ep.03 - Add Makefile backend to SC::Build](https://www.youtube.com/watch?v=wYmT3xAzMxU)
- [Ep.04 - Start Linux Porting](https://www.youtube.com/watch?v=DUZeu6VDGL8)
- [Ep.05 - Build Everything on Linux](https://www.youtube.com/watch?v=gu3x3Y1zZLI)

# Blog

Some relevant blog posts are:

- [March 2024 Update](https://pagghiu.github.io/site/blog/2024-03-27-SaneCppLibrariesUpdate.html)
- [May 2024 Update](https://pagghiu.github.io/site/blog/2024-05-31-SaneCppLibrariesUpdate.html)
- [June 2024 Update](https://pagghiu.github.io/site/blog/2024-06-30-SaneCppLibrariesUpdate.html)
- [July 2024 Update](https://pagghiu.github.io/site/blog/2024-07-31-SaneCppLibrariesUpdate.html)
- [February 2025 Update](https://pagghiu.github.io/site/blog/2025-02-28-SaneCppLibrariesUpdate.html)
- [March 2025 Update](https://pagghiu.github.io/site/blog/2025-03-31-SaneCppLibrariesUpdate.html)
- [April 2025 Update](https://pagghiu.github.io/site/blog/2025-04-30-SaneCppLibrariesUpdate.html)
- [August 2025 Update](https://pagghiu.github.io/site/blog/2025-08-31-SaneCppLibrariesUpdate.html)
- [September 2025 Update](https://pagghiu.github.io/site/blog/2025-09-30-SaneCppLibrariesUpdate.html)
- [November 2025 Update](https://pagghiu.github.io/site/blog/2025-11-30-SaneCppLibrariesUpdate.html)
