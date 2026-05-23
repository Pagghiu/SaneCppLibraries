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
shared library. The standalone native backend is now the default day-to-day workflow on macOS, Linux, and Windows,
while the generated-project backends remain available for IDE and project-file flows.

# Description

Build descriptions are regular C++ files, conventionally named `SC-build.cpp`, that are compiled on the fly through the
repository bootstraps `SC.sh` / `SC.bat` or the external-project launchers `SC-build.sh` / `SC-build.bat` /
`SC-build.ps1`.

For the external bootstrap workflow, including vendored, shared-checkout, and standalone-cache usage, see
[External SC::Build Bootstrap](@ref page_build_external).

The same public API serves two workflows:

- Generated backends: emit XCode, Visual Studio, or Make projects into `_Build/_Projects`
- Native backend: invoke compiler, linker, and archiver child processes directly without generating project files first

The repository tool surface is the `SC-build` tool described in [Tools](@ref page_tools). Its normal command-line shape
is:

```text
./SC.sh build compile [workspace:project | project] [options]
./SC.sh build run [workspace:project | project] [options] [-- extra args...]
./SC.sh build coverage [workspace:project | project] [options]
```

Generated-project workflows also use:

```text
./SC.sh build configure [workspace:project | project]
```

Named options accepted by `compile`, `run`, and `coverage` are:

- `-c`, `--config <NAME>`
- `-g`, `--generator <NAME>`
- `-a`, `--arch <NAME>`
- `--target <PROFILE>` (`compile` and `run` only)
- `--toolchain <NAME>`
- `--triple <VALUE>`
- `--sysroot <PATH>`
- `--runner <MODE>` (`run` only)
- `--runner-path <PATH>` (`run` only)
- `-o`, `--output <MODE>` (`compile` and `run` only)
- `-q`, `--quiet` (`compile` and `run` only)
- `--normal` (`compile` and `run` only)
- `-v`, `--verbose` (`compile` and `run` only)

Generator values accepted by the tool are:

- `default`
- `native`
- `make`
- `xcode`
- `vs2022`
- `vs2019`

Architecture values accepted by the tool are:

- `arm64`
- `intel64`
- `intel32`
- `wasm`
- `any`

Target profiles accepted by the tool today are:

- `host`
- `native`
- `linux-glibc-x86_64`
- `linux-glibc-arm64`
- `linux-musl-x86_64`
- `linux-musl-arm64`
- `windows-gnu-x86_64`
- `windows-msvc-x86_64`
- `windows-msvc-arm64`
- `windows-gnu-arm64`

Runner values accepted by `build run` today are:

- `auto`
- `none`
- `wine`
- `qemu`
- `custom`

Toolchain values accepted by `compile` and `run` today are:

- `default`
- `host-default`
- `clang`
- `filc`
- `gcc`
- `msvc`
- `clang-cl`
- `llvm-mingw`

Current CLI behavior:

- If no project is specified, `compile` builds the whole default workspace
- `run` requires a single executable target and forwards arguments placed after `--`
- If no configuration is specified, `Debug` is used
- Host-default generator selection is `native` on Windows, macOS, and Linux
- `configure` is primarily for generated backends; normal native `compile` / `run` flows build directly into `_Build/_Outputs` and `_Build/_Intermediates`
- `compile` / `run` accept `quiet`, `normal`, or `verbose` output control through `--output` or the `--quiet` / `--normal` / `--verbose` shortcuts
- `--target` selects a friendly host or cross target profile without requiring the caller to spell the toolchain triple manually
- `--toolchain` selects the compiler family orthogonally to `--target`
- `--abi` is reserved for a future public ABI selector; use `--target` for glibc, musl, GNU, or MSVC selection today
- `--triple` and `--sysroot` are raw escape hatches for advanced toolchain overrides
- Raw overrides are applied after `--target`, so they win over the friendly profile defaults
- `build run` can use `--runner` / `--runner-path` to control how foreign executables are launched
- `--runner auto` prefers direct execution for runnable native Linux outputs before falling back to Wine or QEMU
- Contradictory explicit combinations such as mismatched `--generator`, `--arch`, `--runner`, or `--triple` values now fail early with concrete CLI errors
- Legacy positional compatibility is still supported after `target` as `[configuration] [generator] [architecture] [output-mode]`

This is the repository `Tools/SC-build.cpp` file used to configure the default workspace:
\include Tools/SC-build.cpp

# Public API

The public API in `Tools/SC-build/Build.h` currently exposes:

- `Definition`, `Workspace`, `Project`, and `Configuration` for the build graph
- `Libraries::{SingleFile, Multiple}` plus `addSaneCppLibraries(...)` for adding Sane C++ Libraries to a target
- `Machine` and `TargetEnvironment` for explicit host / target modeling
- `TargetType::{ConsoleExecutable, GUIApplication, SharedLibrary, StaticLibrary}`
- `Generator::{Native, XCode, VisualStudio2022, VisualStudio2019, Make}`
- `Toolchain::{HostDefault, Clang, FilC, GCC, MSVC, ClangCL, LLVMMingw, CustomDriver}`
- `RunnerSpec::{Auto, None, Wine, QEMU, Custom}`
- `SupportMatrixEntry`, `SupportStatus`, `SupportTier`, and `getNativeBackendSupportMatrix()` for the native-backend
  cross-compilation support matrix
- `ExecutionOptions` for native-backend parallelism / verbosity knobs
- `OutputMode::{Quiet, Normal, Verbose}` and `ExecutionOptions::outputMode` for native-backend presentation control
- `Directories::projectDirectory` for the root of the project being configured, distinct from the SaneCppLibraries checkout
- `Project::addSpecificFileFlags` for per-file flag groups
- `Project::addExportLibraries`, `addExportAllLibraries`, and `addExportDirectories` for plugin host exports
- `Project::saneCpp` and `Configuration::saneCpp` for Sane C++ Libraries specific policy flags such as strict
  no-standard-library mode

# Consuming Sane C++ Libraries

External `SC-build.cpp` files can add Sane C++ Libraries to a project with:

```cpp
SC_TRY(addSaneCppLibraries(project, parameters));
```

This defaults to `Libraries::SingleFile`, which adds `SC.cpp`.

To compile the individual library sources instead, use:

```cpp
SC_TRY(addSaneCppLibraries(project, parameters, Libraries::Multiple));
```

Call the helper after setting the project root directory, typically with
`project.setRootDirectory(parameters.directories.projectDirectory.view())`.

SC-build is a general-purpose build system, so generic C++ standard-library policy lives in generic compile/link flags.
Normal mode is the default: standard C/C++ headers are available and the C++ standard-library runtime may be linked.
Sane C++ Libraries specific options are grouped under `saneCpp` and are enabled automatically by
`addSaneCppLibraries()`.

To request the stricter historical no-stdlib pressure mode for an entire Sane C++ target:

```cpp
project.files.compile.includeStdCpp = false;
project.link.linkStdCpp = false;
project.saneCpp.provideCppRuntimeShims = true;
```

For a single configuration:

```cpp
configuration.compile.includeStdCpp = false;
configuration.link.linkStdCpp = false;
configuration.saneCpp.provideCppRuntimeShims = true;
```

The generic `includeStdCpp` flag controls whether the backend passes best-effort no-standard-C++-include flags such as
`-nostdinc++`. When `saneCpp.enabled` is true, SC-build also emits `SC_INCLUDE_STD_CPP` from that generic compile flag.

Targets that intentionally use STL runtime features can opt into C++ standard-library linkage:

```cpp
project.link.linkStdCpp = true;
```

If a Sane C++ target avoids linking the C++ runtime, it must either provide the runtime ABI shims itself or obtain them
from another object/library:

```cpp
project.link.linkStdCpp = false;
project.saneCpp.provideCppRuntimeShims = true;
```

SC-build rejects `project.link.linkStdCpp = true` together with `project.saneCpp.provideCppRuntimeShims = true` because
both sides would be trying to provide the same C++ runtime ABI symbols.

The repository bootstrap keeps broad compiler compatibility for headers by default while still avoiding C++ runtime
linkage. Set `SC_BOOTSTRAP_INCLUDE_STD_CPP=0` only when you intentionally want to test or benchmark strict-mode
bootstrap compilation. Set `SC_BOOTSTRAP_LINK_STD_CPP=1` if you explicitly want the bootstrap tool to link the C++
standard-library runtime.

# Backend Matrix

| Backend | Platforms | Target kinds | `compile_commands.json` | Notes |
|:--|:--|:--|:--|:--|
| `Native` | macOS, Linux, Windows hosts | Console executable, GUI application, shared library, static library | Yes | Direct compile / run / coverage flow; no project files are generated |
| `XCode` | Apple | Console executable, GUI application, shared library, static library | Yes | GUI apps also emit storyboard / assets; `Intel32` and `Wasm` architectures are unsupported |
| `VisualStudio2022` / `VisualStudio2019` | Windows | Console executable, GUI application, shared library, static library | No | Explicit generated-project workflow for Visual Studio |
| `Make` | Apple, Linux | Console executable, GUI application, shared library, static library | Yes on clang-style flows | Explicit generated-project workflow for Make; GCC builds still compile, but Makefile-side compile database generation is unavailable there |

# Native Cross Support Matrix

The native backend exposes its current cross-compilation support claims through
`Build::getNativeBackendSupportMatrix()`. Documentation and CI should treat that API as the source of truth for
host / target rows, build support, run support, support tier, runner family, and validation notes.

| Host | Target | Architecture | Build | Run | Runner | Tier |
|:--|:--|:--|:--|:--|:--|:--|
| macOS | windows-gnu | x86_64 | supported | supported | wine | tier1 |
| macOS | windows-gnu | arm64 | supported | not-yet | wine | tier1 |
| macOS | windows-msvc | x86_64 | supported | smoke-supported | wine | tier2 |
| macOS | windows-msvc | arm64 | supported | not-yet | wine | tier2 |
| macOS | linux-glibc | x86_64 | supported | not-yet | qemu | tier1 |
| macOS | linux-glibc | arm64 | supported | not-yet | qemu | tier1 |
| macOS | linux-musl | x86_64 | supported | not-yet | qemu | tier1 |
| macOS | linux-musl | arm64 | supported | not-yet | qemu | tier1 |
| Windows | linux-glibc | arm64 | supported | not-yet | qemu | tier2 |
| Windows | linux-musl | x86_64 | supported | not-yet | qemu | tier2 |
| Linux | windows-gnu | x86_64 | supported | supported | wine | tier1 |
| Linux | windows-gnu | arm64 | supported | smoke-supported | wine | tier1 |
| Linux | windows-msvc | x86_64 | supported | smoke-supported | wine | tier2 |
| Linux | windows-msvc | arm64 | supported | smoke-supported | wine | tier2 |

# Native Backend

The standalone backend is selected through `SC::Build::Generator::Native`.

Current implemented scope:

- Host platforms: macOS, Linux and Windows
- Toolchain families exposed by the API: `HostDefault`, `Clang`, `FilC`, `GCC`, `MSVC`, `ClangCL`, `LLVMMingw`, and `CustomDriver`
- Experimental compiler-first track: `FilC` is now exposed by the API and CLI through `--toolchain filc`, but it is Linux-only, compiler-first, and toolchain-only for now; no public `linux-filc-*` target profile exists
- Target kinds: console executables, GUI applications, shared libraries, and static libraries
- Dependency tracking: compiler-generated dependency files / dependency output
- Incrementality: skips up-to-date compile and link steps
- Workspace dependency ordering: native executable targets can link workspace-local static libraries by name
- Parallelism: compile fan-out is controlled by `ExecutionOptions::maxParallelJobs`
- Output modes: `quiet` hides progress lines and successful child output, `normal` shows progress plus grouped failures and summaries, and `verbose` also shows rebuild traces, skip lines, and successful compile output
- Output layout: `_Build/_Outputs`, `_Build/_Intermediates`, and `_Build/_BuildCache`

Current cross-compilation scope:

- macOS and Linux hosts can compile `windows-gnu-x86_64` and `windows-gnu-arm64` through packaged `llvm-mingw`
- Linux hosts can now experiment with Fil-C through `SC-package install filc` plus `build ... --toolchain filc`; this is currently limited to native Linux `x86_64` output and remains toolchain-only outside the public target-profile matrix
- Linux `glibc` and `musl` target profiles now exist as first-class native-backend profiles; they shape canonical target
  triples and sysroot flags, and macOS hosts now auto-select both a packaged LLVM toolchain and packaged Linux sysroots
  for them when the caller has not provided explicit compiler paths
- macOS now has real native-backend `SCTest` compile validation for `linux-glibc-x86_64`, `linux-glibc-arm64`,
  `linux-musl-x86_64`, and `linux-musl-arm64`
- `build run` can now wrap foreign Linux targets through `qemu-user`; the native runner can reuse a managed
  `SC-package install qemu` registration or fall back to host `qemu-*` executables from `PATH`, and it passes
  `-L <sysroot>` so dynamically linked Linux targets can find their loader and libraries
- macOS glibc and musl Linux target runs are still `not-yet` in the support matrix; real host-QEMU validation remains
  opportunistic until CI can acquire `qemu-x86_64` and `qemu-aarch64` deterministically
- Windows hosts have packaged LLVM + Linux sysroot compile validation for representative `linux-glibc-arm64` and
  `linux-musl-x86_64` target profiles; Windows-host Linux runs remain pending
- macOS hosts can acquire a portable MSVC + Windows SDK package with `./SC.sh package install msvc` and compile `windows-msvc-x86_64` and `windows-msvc-arm64` through the native backend
- Linux arm64 hosts can now validate the same portable MSVC path end-to-end for `windows-msvc-x86_64` and
  `windows-msvc-arm64`; the package tool auto-prefers a generated `box64 + wine64` wrapper when those host tools are
  installed, can fall back to an auto-installed packaged Linux Wine runner that resolves a maintained generic-arm
  `box64` build when system `box64` is absent, and native `build run` can now auto-install a separate ARM64 Wine
  runtime for `windows-*-arm64` execution while still accepting plain `wine64` / `wine` or an explicit `--wine` /
  `SC_MSVC_WINE` override
- `SC-package install msvc` also accepts explicit `--import-directory <path>` and `--wine <path>` overrides so imported
  layouts and custom Wine wrappers no longer have to be driven only through environment variables
- Once a portable MSVC package is installed, later native-backend Windows MSVC builds can reuse the recorded wrapper path
  from `sc-msvc-package.json` instead of requiring `SC_MSVC_WINE` or host Wine discovery again
- Existing portable MSVC layouts can now repair missing `sc-msvc-package.json` metadata and wrapper scripts in place, and
  SDK version detection now falls back from `Windows Kits/10/bin` to `Include` or `Lib` when the SDK tools directory is
  absent
- Existing packaged Linux Wine runners now also repair their launcher scripts in place, so later portable MSVC wrapper
  updates do not require deleting the cached runner package first
- Portable MSVC caches are now host-specific (`macOS` vs `Linux`) so shared workspaces do not reuse the wrong recorded
  Wine runner path across hosts
- On Linux arm64, the packaged MSVC validation story is now real for both target architectures:
  `windows-msvc-x86_64` has a clean native-backend `SCTest` compile validation plus a targeted `BaseTest/new-delete`
  run through the maintained packaged Box64 runner, and `windows-msvc-arm64` now also has a clean native-backend
  `SCTest` compile plus a targeted `BaseTest/new-delete` run through an auto-installed native ARM64 Wine runner
- On Linux arm64, `build run` now keeps Windows console targets on plain packaged `wine` instead of auto-switching to
  `wineconsole`, because the current Box64 Wine console path is less reliable than plain `wine` on this host
- `build run` can auto-route `windows-gnu-x86_64` executables through Wine on macOS and Linux, and Linux arm64 now
  also smoke-runs `windows-gnu-arm64` through the packaged native ARM64 Wine runner
- On Linux arm64, that same native `build run` path now auto-prefers generated `box64 + wine64` wrappers when the host
  provides those commands, and console targets still switch to a sibling `wineconsole --backend=curses` wrapper when it
  is available on Linux x64 hosts
- The current MSVC-via-Wine validation target is still narrower than the Windows GNU path: macOS has a real compile,
  link, and tiny-start smoke for `windows-msvc-x86_64`, while Linux arm64 now has targeted `SCTest`
  `BaseTest/new-delete` smokes for both `windows-msvc-x86_64` and `windows-msvc-arm64`
- `build run` now routes Wine launches through `cmd /c` with Windows-style paths, which fixes real macOS Wine startup for `windows-gnu-x86_64`
- `windows-gnu-arm64` and `windows-msvc-arm64` are no longer blocked by a hardcoded CLI rule; Linux arm64 now has a
  real ARM64-capable Wine runtime for targeted smokes, while the packaged macOS Wine runner still does not ship an
  ARM64 Windows loader
- Cross-target plugin tests remain out of scope for now because the current plugin test flow assumes MSVC-oriented Windows behavior

Typical native commands:

```bash
./SC.sh build compile SCBuildTest --config Debug
./SC.sh build compile SCBuildTest -c d -a arm64 --verbose
./SC.sh package install llvm
./SC.sh package install qemu --import-directory /opt/qemu-user
./SC.sh package verify llvm
./SC.sh build compile SCTest --target linux-glibc-arm64 --output quiet
./SC.sh build compile SCTest --target linux-musl-x86_64 --output quiet
./SC.sh build run SCTest --arch intel64 --runner auto -- --test BaseTest
./SC.sh build compile SCTest --target windows-gnu-x86_64 --output quiet
./SC.sh build compile SCTest --target windows-gnu-arm64 --output quiet
./SC.sh package install filc --import-directory /home/user/filc-0.678-linux-x86_64
./SC.sh build compile SaneHttpGet --toolchain filc --output quiet
./SC.sh package install msvc
./SC.sh package install msvc --import-directory /opt/msvc-portable --wine /opt/bin/wine-wrapper
./SC.sh build compile SCTest --target windows-msvc-x86_64 --output quiet
./SC.sh build compile SCTest --target windows-msvc-arm64 --output quiet
./SC.sh build compile SCTest --target windows-gnu-x86_64 --triple x86_64-custom-windows-gnu --sysroot /opt/sysroots/windows
./SC.sh build run SCTest --target windows-gnu-x86_64 --runner auto -- --test BaseTest --test-section new/delete
./SC.sh build run SCBuildTest --config Debug -- --test "BuildTest"
SC.bat build compile SCTest --config Debug
SC.bat build compile SCTest --target linux-glibc-arm64 --output quiet
SC.bat build compile SCTest --target linux-musl-x86_64 --output quiet
```

Important current limits:

- The QEMU runner path can reuse a managed imported `qemu-user` layout or host `qemu-*` executables; real host-QEMU
  smoke validation remains opportunistic on macOS until QEMU-user acquisition is deterministic in CI, so macOS Linux
  run support stays `not-yet`
- Windows-host Linux-target support is compile-only today and validated for representative glibc arm64 and musl x86_64
  slices; Windows-host Linux run validation is still pending
- Fil-C is still an experimental compiler-first Linux track: the public shape is `--toolchain filc`, no public `linux-filc-*` target profile exists, Linux `x86_64` is the only intended output slice for the first milestone, and Linux arm64 hosts may still require imported local installs plus host-specific translation/linker helpers during validation
- Windows native sysroot selection is not implemented yet
- `run` is valid only for executable targets and only when a single project is selected
- The repository `build configure` command remains available for generated-project workflows; native builds do not need
  generated project files first

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

# `SC_BUILD` Define

When `SC-build.cpp` is compiled as the build-definition tool, `SC::Build` defines `SC_BUILD=1` for that compilation.

This allows one `SC-build.cpp` file to provide `SC::Build::configure(...)` in the `SC_BUILD` branch and a normal
`main()` in the `#else` branch if the same file is also added to the built target.

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

# Generated Project Workflows

Use `build configure` when you explicitly want generated projects for IDEs or Make:

```bash
./SC.sh build configure
SC.bat build configure
```

Typical examples:

- XCode: open `_Build/_Projects/XCode/SCWorkspace/SCWorkspace.xcworkspace`
- Visual Studio: open `_Build/_Projects/VisualStudio2022/SCWorkspace/SCWorkspace.sln`
- Make: use `_Build/_Projects/Make/SCWorkspace/apple` or `_Build/_Projects/Make/SCWorkspace/linux`

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
- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)
- [April 2026 Update](https://pagghiu.github.io/site/blog/2026-04-30-SaneCppLibrariesUpdate.html)
