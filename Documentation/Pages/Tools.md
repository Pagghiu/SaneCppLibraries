@page page_tools Tools

[TOC]

`SC::Tools` are self contained single C++ source files that are (automatically) compiled on the fly and linked to Sane C++ to be immediately executed.  

They leverage the growing shell, system and network programming capabilities of Sane C++ Libraries, by just including the `SC.cpp` unity build file, that has no third party dependencies (see [Building (Contributor)](@ref page_building_contributor)).  

Another way to look at them is just as small _C++ scripts_ for which you don't need to setup or maintain a build system, as long as you only use Sane C++ Libraries.

@note Name is `SC::Tools` and not `SC::Scripts` because they're still just small programs.  
If the system will be generalized even more, maybe acquiring more advanced capabilities from [SC::Plugin](@ref library_plugin) and [SC::Build](@ref page_build) or sandboxing capabilities, this naming will be re-evaluated and/or changed.

# Blog

Some relevant blog posts are:

- [March 2024 Blog update post](https://pagghiu.github.io/site/blog/2024-03-27-SaneCppLibrariesUpdate.html)
- [April 2025 Update](https://pagghiu.github.io/site/blog/2025-04-30-SaneCppLibrariesUpdate.html)
- [October 2025 Update](https://pagghiu.github.io/site/blog/2025-10-31-SaneCppLibrariesUpdate.html)
- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)

# Reasons

`SC::Tools` has been created for the following reasons:  

- Enjoy the coolness of writing _C++ scripts_
- Create development tools and automate shell operations in a real programming language
- Allow C++ programmers to use regular C++ IDE / Debuggers when writing _automation / shell scripts_
- Use Sane C++ Libraries for real tasks to improve them and fix bugs:
    - Example: `Tools/SC-package.cpp` uses SC::Process library to download some third party binary 
    - Example: `Tools/SC-package.cpp` uses SC::Hashing library to check downloaded package hashes
    - Example: `Tools/SC-format.cpp` uses SC::FileSystemIterator library to find all files to format in the repo
- Create portable scripts that can be written once and run (or be debugged) on all platforms
- Avoid introducing additional dependencies
- Keep the percentage of C++ code in the repo as high as possible (as a consequence of the above)

# Invoking built-in Tools

All built-in tools are invoked with the `SC.sh` or `SC.bat` bootstrap script that is located in the root of the repo.

Such script must be called with the name of the tool and some parameters.  

For example, invoking the `Tools\SC-build.cpp` tool to build a target directly:

```
./SC.sh build compile SCTest
```

or (on Windows)

```
SC.bat build compile SCTest
```

@note `SC::Tools` are just regular programs being compiled on the fly when needed, so they require a working host compiler to be available in system path. This limitation could be removed if needed, as described in the Roadmap section.  

# External SC::Build launcher

External projects should use `SC-build.sh`, `SC-build.bat`, or `SC-build.ps1` instead of the repository-only
`SC.sh build ...` entrypoint.

Those launchers:

- Search upward for a project-local `SC-build.cpp`
- Accept `--project-dir <path>` and `--libraries-root <path>` overrides
- Can use a vendored checkout, an explicit shared checkout, or a shared cached clone plus versioned worktrees

See [External SC::Build Bootstrap](@ref page_build_external) for the full workflow and examples.

# Invoking custom tools

Tools can be automatically compiled and run by just passing its full path to the `SC` bootstrap.

Given the following custom tool at `myToolDirectory/TestScript.cpp`

```cpp
#include "Libraries/Strings/Console.h"
#include "Tools/Tools.h"
namespace SC
{
namespace Tools
{
StringView Tool::getToolName() { return "Test"; }
StringView Tool::getDefaultAction() { return "test"; }
Result     Tool::runTool(Tool::Arguments& toolArgs)
{
    toolArgs.console.printLine("Hey This is the output from My Tool!!!");
    toolArgs.console.printLine(toolArgs.action);
    for(auto arg : toolArgs.arguments)
        toolArgs.console.printLine(arg);
    return Result(true);
}
} // namespace Tools
} // namespace SC


```

This is how to invoke the tool with `action` and some parameters
```
./SC.sh myToolDirectory/TestScript.cpp myAction param1 param2 ..
```

or (on Windows):

```
SC.bat myToolDirectory\TestScript.cpp myAction param1 param2 ...
```

Possible output (Posix):
```
 ~/Developer/Projects/SC > ./SC.sh TestScript.cpp myAction param1 param2
Starting TestScript
Test "myAction" started
librarySource    = "/Users/user/Developer/Projects/SC"
toolSource       = "/Users/user/Developer/Projects/SC/Tools"
toolDestination  = "/Users/user/Developer/Projects/SC/_Build"
Hey This is the output from My Tool!!!
myAction
param1
param2
Test "myAction" finished (took 1 ms)
```

This is the list of tools that currently exist in the Sane C++ repository.

# SC-build.cpp

`SC-build` builds Sane C++ repository targets directly through the standalone native backend by default, and can also
configure generated projects through [SC::Build](@ref page_build).

## Actions

- `configure`: Generates repository projects into `_Build/_Projects/<Generator>/<Workspace>`
- `compile`: Builds one project or an entire workspace through the selected backend
- `run`: Builds a single executable target if needed and then runs it
- `coverage`: Builds clang coverage output into `_Build/_Coverage`
- `documentation`: Builds the documentation into `_Build/_Documentation`

`SC-build` command shape:

```text
./SC.sh build compile [workspace:project | project] [options]
./SC.sh build run [workspace:project | project] [options] [-- extra args...]
./SC.sh build coverage [workspace:project | project] [options]
./SC.sh build configure [workspace:project | project]
```

Generator keywords are `default`, `native`, `make`, `xcode`, `vs2022`, and `vs2019`.

Named options are:

- `-c`, `--config <NAME>`
- `-g`, `--generator <NAME>`
- `-a`, `--arch <NAME>`
- `--target <PROFILE>` (`compile` / `run`)
- `--toolchain <NAME>`
- `--abi <NAME>` (reserved; use `--target` for glibc, musl, GNU, or MSVC today)
- `--triple <VALUE>`
- `--sysroot <PATH>`
- `--runner <MODE>` (`run`)
- `--runner-path <PATH>` (`run`)
- `-o`, `--output <MODE>` (`compile` / `run`)
- `-q`, `--quiet` (`compile` / `run`)
- `--normal` (`compile` / `run`)
- `-v`, `--verbose` (`compile` / `run`)

Target profiles currently exposed by the CLI are `host`, `native`, `linux-glibc-x86_64`, `linux-glibc-arm64`,
`linux-musl-x86_64`, `linux-musl-arm64`, `windows-gnu-x86_64`, `windows-msvc-x86_64`, `windows-msvc-arm64`, and
`windows-gnu-arm64`.

Toolchain keywords currently exposed by the CLI are `default`, `host-default`, `clang`, `filc`, `gcc`, `msvc`,
`clang-cl`, and `llvm-mingw`.

Runner keywords currently exposed by the CLI are `auto`, `none`, `wine`, `qemu`, and `custom`.

`--abi` is intentionally reserved for a future public ABI selector. `--triple` and `--sysroot` are the current raw
escape hatches for advanced toolchain overrides. When combined with `--target`, the raw overrides win.

Contradictory explicit combinations such as mismatched `--generator`, `--arch`, `--runner`, or `--triple` values now
fail early with concrete CLI errors instead of drifting into backend-time failures.

Current defaults:

- Windows / macOS / Linux: `default` resolves to `native`
- `SC-build` defaults to the `compile` action when no action is passed
- Native `compile` / `run` are the standard workflows and do not require a prior `configure` step
- `configure` is for generated-project and IDE workflows
- Linux `glibc` and `musl` target profiles now shape canonical target triples and sysroot flags; macOS and Windows
  hosts auto-select a packaged LLVM toolchain for those profiles when the caller does not provide explicit compiler
  paths, macOS auto-selects packaged Linux glibc/musl sysroots for them, and Windows has representative packaged
  sysroot compile validation for `linux-glibc-arm64` and `linux-musl-x86_64`
- macOS now has real native-backend `SCTest` compile validation for `linux-glibc-x86_64`, `linux-glibc-arm64`,
  `linux-musl-x86_64`, and `linux-musl-arm64`
- macOS and Linux native builds can cross-compile Windows GNU `x86_64` and `arm64` targets through packaged `llvm-mingw`
- Linux native builds can now also experiment with Fil-C through `SC-package install filc` plus `build ... --toolchain filc`; this is compiler-first Linux support for native `x86_64` output only and is toolchain-only for now, with no public `linux-filc-*` target profile
- macOS native builds can also acquire a portable MSVC + Windows SDK package with `./SC.sh package install msvc` and
  compile `windows-msvc-x86_64` and `windows-msvc-arm64`
- Linux arm64 hosts can now validate the portable MSVC path end-to-end for `windows-msvc-x86_64` and
  `windows-msvc-arm64`; `./SC.sh package install msvc` auto-prefers a generated `box64 + wine64` wrapper when those
  tools are available, can fall back to an auto-installed packaged Linux Wine runner that resolves a maintained
  generic-arm `box64` build when system `box64` is absent, and native `build run` can now auto-install a separate
  ARM64 Wine runtime for `windows-*-arm64` execution while still accepting plain `wine64` / `wine` or an explicit
  `--wine` / `SC_MSVC_WINE` override
- `SC-package install msvc` now also accepts `--import-directory <path>` and `--wine <path>` so imported layouts and
  custom Wine wrappers can be selected directly from the command line
- Once that package is installed, later native `windows-msvc-*` builds can reuse the recorded wrapper path from
  `sc-msvc-package.json` instead of depending on `SC_MSVC_WINE` or host Wine discovery again
- Existing portable MSVC layouts can now repair missing `sc-msvc-package.json` metadata and wrapper scripts in place,
  and SDK version detection now falls back from `Windows Kits/10/bin` to `Include` or `Lib` when SDK tools are absent
- Existing packaged Linux Wine runners now also repair their launcher scripts in place, so portable MSVC wrapper
  updates do not require deleting the cached runner package first
- Portable MSVC caches are now host-specific (`macOS` vs `Linux`) so shared workspaces do not reuse the wrong recorded
  Wine runner path across hosts
- On Linux arm64, the real portable MSVC path now reaches clean native-backend `SCTest` compiles for both
  `windows-msvc-x86_64` and `windows-msvc-arm64`, plus targeted `BaseTest/new-delete` runs for both targets through
  the maintained packaged Box64 runner (`x86_64`) and the packaged native ARM64 Wine runner (`arm64`)
- Foreign Linux targets can now use `--runner qemu` or `--runner auto` to wrap `build run` through a managed
  `SC-package install qemu` registration or a host `qemu-user` executable from `PATH`; the runner passes
  `-L <sysroot>` so the Linux loader and runtime libraries resolve from the selected sysroot
- macOS `linux-glibc-*` and `linux-musl-*` runs are smoke-supported when real host `qemu-x86_64` and `qemu-aarch64`
  executables are available in CI
- `build run` can auto-route `windows-gnu-x86_64` through Wine on macOS and Linux, and Linux arm64 now also
  smoke-runs `windows-gnu-arm64` through the packaged native ARM64 Wine runner
- On Linux arm64, that same native runner path now auto-prefers generated `box64 + wine64` wrappers when those host
  tools are available; Linux x64 console targets still switch to a sibling `wineconsole --backend=curses` wrapper when
  it is present, while Linux arm64 now stays on plain packaged `wine` because that is more reliable than `wineconsole`
  on the current Box64 stack
- The current macOS `windows-msvc-x86_64` validation scope is a real compile, link, and tiny-start smoke through Wine;
  Linux arm64 now also has targeted `SCTest` smokes for both `windows-msvc-x86_64` and `windows-msvc-arm64`
- `build run` now launches Wine through `cmd /c` with a Windows-style target path, which fixes real macOS startup for
  `windows-gnu-x86_64`
- `windows-gnu-arm64` and `windows-msvc-arm64` are no longer rejected by a hardcoded CLI rule; Linux arm64 now has a
  real ARM64-capable Wine runtime for targeted smokes, while the packaged macOS Wine runner still lacks an ARM64
  Windows loader
- Legacy positional compatibility is still accepted after `target` as `[config] [generator] [arch] [output]`

## Examples
Generate explicit project files:
```
./SC.sh build configure
```
Possible Output:

```
 ~/Developer/Projects/SC > ./SC.sh build configure
Compiling SC-build.cpp
Linking SC-build
Starting SC-build
SC-build "configure" started
librarySource    = "/Users/user/Developer/Projects/SC"
toolSource       = "/Users/user/Developer/Projects/SC/Tools"
toolDestination  = "/Users/user/Developer/Projects/SC/_Build"
projects         = "/Users/user/Developer/Projects/SC/_Build/_Projects"
outputs          = "/Users/user/Developer/Projects/SC/_Build/_Outputs"
intermediates    = "/Users/user/Developer/Projects/SC/_Build/_Intermediates"
SC-build "configure" finished (took 72 ms)
 ~/Developer/Projects/SC > 
```

Build all projects
```
./SC.sh build compile
```

Build through the default native backend
```
./SC.sh build compile SCTest --config Debug
```

Prepare the packaged host LLVM toolchain for Linux targets
```
./SC.sh package install llvm
```

Register an imported QEMU user-runner layout for foreign Linux `build run`
```
./SC.sh package install qemu --import-directory /opt/qemu-user
```

Register an imported Fil-C installation and compile a native Linux target through the experimental compiler-first path
```
./SC.sh package install filc --import-directory /home/user/filc-0.678-linux-x86_64
./SC.sh build compile SaneHttpGet --toolchain filc --output quiet
```

Cross-compile a Linux glibc executable through the native backend with the packaged macOS sysroot path
```
./SC.sh build compile SCTest --target linux-glibc-arm64 --output quiet
```

Cross-compile a Linux musl executable through the native backend with the packaged macOS sysroot path
```
./SC.sh build compile SCTest --target linux-musl-x86_64 --output quiet
```

Cross-compile a Windows GNU executable through the native backend
```
./SC.sh build compile SCTest --target windows-gnu-x86_64 --output quiet
```

Acquire the portable MSVC package and cross-compile a Windows MSVC executable on macOS
```
./SC.sh package install msvc
./SC.sh build compile SCTest --target windows-msvc-x86_64 --output quiet
```

Cross-compile a Windows MSVC arm64 executable on macOS
```
./SC.sh package install msvc
./SC.sh build compile SCTest --target windows-msvc-arm64 --output quiet
```

Cross-compile a Windows GNU arm64 executable
```
./SC.sh build compile SCTest --target windows-gnu-arm64 --output quiet
```

Cross-compile through a friendly profile and then override the raw toolchain details
```
./SC.sh build compile SCTest --target windows-gnu-x86_64 --triple x86_64-custom-windows-gnu --sysroot /opt/sysroots/windows
```

Smoke-run a Windows GNU `x86_64` executable through the shared runner model
```
./SC.sh build run SCTest --target windows-gnu-x86_64 --runner auto -- --test "BaseTest" --test-section "new/delete"
```

Run a generated-backend executable and pass extra test arguments after `--`
```
SC.bat build run SCTest --config Debug --generator vs2022 -- --test "ThreadingTest"
```

Open generated projects:

- XCode: `_Build/_Projects/XCode/SCWorkspace/SCWorkspace.xcworkspace`
- Visual Studio 2022: `_Build/_Projects/VisualStudio2022/SCWorkspace/SCWorkspace.sln`
- Visual Studio 2019: `_Build/_Projects/VisualStudio2019/SCWorkspace/SCWorkspace.sln`
- Make (macOS): `_Build/_Projects/Make/SCWorkspace/apple`
- Make (Linux): `_Build/_Projects/Make/SCWorkspace/linux`

Possible Output:
```
~/Developer/Projects/SC > ./SC.sh build compile     
Starting SC-build
SC-build "compile" started
librarySource    = "/Users/user/Developer/Projects/SC"
toolSource       = "/Users/user/Developer/Projects/SC/Tools"
toolDestination  = "/Users/user/Developer/Projects/SC/_Build"
projects         = "/Users/user/Developer/Projects/SC/_Build/_Projects"
outputs          = "/Users/user/Developer/Projects/SC/_Build/_Outputs"
intermediates    = "/Users/user/Developer/Projects/SC/_Build/_Intermediates"
/Applications/Xcode.app/Contents/Developer/usr/bin/make clean
Cleaning SCTest
Creating /Users/user/Developer/Projects/SC/_Build/_Projects/Make/../../_Intermediates/SCTest/macOS-arm64-make-clang-Debug
Creating /Users/user/Developer/Projects/SC/_Build/_Projects/Make/../../_Outputs/macOS-arm64-make-clang-Debug
Compiling Async.cpp
Compiling AsyncTest.cpp
Compiling BuildTest.cpp
... other files..
Compiling DebugVisualizersTest.cpp
Compiling SC-format.cpp
Compiling ToolsTest.cpp
Compiling SC-package.cpp
Generate compile_commands.json
Linking SCTest
SC-build "compile" finished (took 3071 ms)
```

# SC-package.cpp

`SC-package` downloads third party tools needed for Sane C++ development (example: `clang-format`).  
Proper OS / Architecture combination is selected (Windows/Linux/Mac and Intel/ARM) and all downloaded files (_packages_) are hash checked for correctness.  
_Packages_ are placed and extracted in `_Build/_Packages` while downloaded archives are reused from `_Build/_PackagesCache`.  

@note Directory naming has been chosen to avoid clashes when mounting the same working copy folder in multiple concurrent virtual machines with different Operating Systems / architectures.  
This happens during regular development, where new code is frequently tested in parallel on macOS, Windows and Linux before even committing it and pushing it to the CI system.

## Actions

- `install`: Downloads required tools (LLVM archives / documentation tools)
- `list`: Lists known packages from the built-in package registry
- `help`: Prints package actions and import options
- `info <package>`: Prints package metadata such as kind, installed name, variants, source, exports, phases and import support
- `status [package]`: Reports whether one package, or all known packages, have structured install receipts and whether
  each found receipt validates
- `verify [package]`: Checks one package, or all installed receipts, and validates exported paths
- `receipt <package>`: Prints the installed package root, receipt path and raw `sc-package-receipt.json` contents
- `exports <package>`: Prints the exports recorded by the installed receipt with resolved paths
- `lock`: Writes a local `_Build/SC-package.lock` summary of installed package receipts

## Examples

```
./SC.sh package install
./SC.sh package help
./SC.sh package list
./SC.sh package info llvm
./SC.sh package status llvm
./SC.sh package verify llvm
./SC.sh package receipt llvm
./SC.sh package exports llvm
```

Installed packages now write a `sc-package-receipt.json` file next to the package root. The receipt records the schema,
package identity, recipe version, host platform, variant, source provenance, optional source hash, install root,
validation result, named phases and exported tools / sysroots / runners / include directories / library directories.
`SC-build` consumes these exports for packaged LLVM, Linux sysroots, QEMU, Wine, Fil-C, llvm-mingw and portable MSVC
resolution, while preserving the older layout-based fallback during migration.

Simple archive/git packages are routed through an internal C++ recipe lifecycle. The recipe holds package source,
extraction/test hooks, named phases and exports. Complex packages such as MSVC, Wine, QEMU, Fil-C and Linux sysroots
still use custom C++ adapters, but their imperative parts are now named in the registry and receipts (for example
`resolveImportedQEMU`, `prepareFilCCompilerLaunchers`, `resolveLinuxSysrootPackages`, `repairMSVCLayout` and
`validateMSVCLayout`). This keeps the special behavior visible without introducing an external recipe DSL yet.

External tools can compose their own package registry in C++ and reuse the built-in catalog when desired:

```cpp
PackageRegistryEntry entries[16];
PackageRegistryBuilder registryBuilder = {{entries, 16}};
SC_TRY(addBuiltinPackages(registryBuilder));
SC_TRY(registryBuilder.add(myPackageEntry));

Tools::Package package;
SC_TRY(runPackageTool(arguments, registryBuilder.registry(), &package));
```

The builder uses caller-owned storage so registry entry lifetimes are explicit. A custom entry can provide its own
`PackageInstallHandler` today; future recipe descriptors and named phase hooks should make the common cases data-only.

`status` reports receipt validity without failing the command, while `verify` fails on invalid receipts after checking
receipt shape, source-hash syntax and exported paths. `lock` writes `_Build/SC-package.lock` with the
resolved package name, version, recipe version, host platform, variant, source, hash, install root, receipt path and
exports for each installed receipt, making package state auditable without making imported packages look
content-addressed.

## Packages
These are the packages that are currently downloaded and extracted / symlinked by `SC-package.cpp`:

- `LLVM 20.1.8`: Downloads `clang-format` from the official LLVM github repository using SHA256 pinned archives
- `LLVM toolchain 20.1.8`: Downloads the full host LLVM toolchain (`clang`, `clang++`, `llvm-ar`, `lld`) for
  Linux-target native-backend flows on supported hosts
- `Fil-C 0.678` (experimental): Linux-only compiler package with a pinned upstream `pizfix` download path plus
  `--import-directory <path>` registration for compiler-first native-backend work through `--toolchain filc`
- `doxygen`: Doxygen documentation generator
- `doxygen-awesome-css`: Doxygen awesome css (Doxygen theme)

@note Automatic `clang-format` install is available on macOS ARM64, Linux ARM64/x64 and Windows ARM64/x64.  
Intel macOS users should install `llvm@20` manually (for example through Homebrew) because recent official LLVM releases no longer provide Intel macOS archives.

# SC-format.cpp

`SC-format` tool formats or checks formatting for all files in Sane C++ repository

## Actions
- `execute`: Formats all source files in the repository in-place
- `check`: Does a dry-run format of all source files. Returns error on unformatted files (used by the CI)

## Examples

Format all source files in the repository
```
./SC.sh format execute
```

Check if all source files in the repository are properly formatted
```
SC.bat format check
```

# How does it work

The bootstrap batch (`SC.bat`) / bash (`SC.sh`) shell scripts are doing the following:  
- Two `.cpp` files are compiled (only if they're out of date) by a `Makefile`
    - `Tools/Tools.cpp` file 
        - Includes the `SC.cpp` unity build file, defining `main(argc, argv)`
    - `$TOOL.cpp` file
        - Implements the specific tool functionality
- The first argument after `$TOOL` name is the main `$ACTION` by convention
- `$ACTION` and all `$PARAM_X` arguments passed to the bootstrap shell scripts are forwarded to the tool
- The resulting executable is run with the forwarded arguments

Example:  
`SC.sh $TOOL $ACTION $PARAM_1 $PARAM_2 ... $PARAM_N`

@note `Tools.cpp` is shared between all compiled tools by the `Makefile` inside the `_Build/_Tools/_Intermediates` directory.  
This allows modifying and recompiling a script, or compiling different scripts spending negligible (often sub-second) time, to just compile/link script logic.  
This is because all Sane C++ Libraries are compiled just once (the first time, in no more than a couple of seconds) inside `Tools.cpp`.

# Bootstrap Sequence

SC::Tools define an handy way of invoking an on the fly compiled program that has Sane C++ Libraries.
The bootstrap process bringing from the command line to final execution is the following:

1. The user invokes a tool like for example: `./SC.sh build compile SCTest Debug`
2. The first argument to the `SC.sh` is the `${TOOL_NAME}` (in this case `build`)
3. `SC.sh` checks if `Tools/ToolsBootstrap` executable exists and is up to date with `Tools/ToolsBootstrap.c`
4. If not, it compiles `Tools/ToolsBootstrap.c` to create the bootstrap executable
5. `SC.sh` invokes `Tools/ToolsBootstrap` with the library directory, tool source directory, build directory, tool name, and any additional arguments
6. A similar process occurs on Windows with `SC.bat` setting up the MSVC environment and compiling `Tools/ToolsBootstrap.exe` if needed
7. `ToolsBootstrap` receives 3 predefined arguments:
    a. The directory of SC Libraries `${LIBRARY_DIR}`
    b. The directory containing Tools `${TOOL_SOURCE_DIR}`
    c. The build products directory `${BUILD_DIR}` containing intermediates and final products
8. Additional arguments are the tool name and action/parameters
9. `ToolsBootstrap` checks if the selected tool executable exists and is up to date
10. If not, `ToolsBootstrap` compiles `Tools/Tools.cpp` and the selected tool source (for example `Tools/SC-build.cpp`)
11. `ToolsBootstrap` invokes the compiled tool with the original action and parameters
12. The `SC-build` tool then configures projects, compiles, runs, or performs the selected action


# Roadmap

- Generate ready made .vscode configurations to debug the programs easily
- As the native backend in [SC::Build](@ref page_build) matures across all supported platforms, reduce and eventually remove the tool `Tools\Build\$(PLATFORM)` makefiles.
- Investigate better way of expressing the dependencies chain between scripts
- Investigate if Tools (scripts) can be sandboxed using os facilities
- Do not require to have a C++ toolchain already installed on the system [*]

[*] Running the Sane C++ Tools scripts on a machine without a pre-installed compiler could be implemented by shipping a pre-compiled `SC-package.cpp` (that is already capable of downloading `clang` on all platforms) or by adding to the bootstrap script ability to download a suitable compiler (and sysroot).
