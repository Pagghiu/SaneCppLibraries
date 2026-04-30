@page page_building_contributor Building (Contributor)

Follow this guide if you're interested in building the library to contribute (check [CONTRIBUTING.md](https://github.com/Pagghiu/SaneCppLibraries/blob/main/CONTRIBUTING.md)! and [Coding Style](@ref page_coding_style)) or you're simply curious to run the test suite.

[TOC]

# Build and run directly

The test suite and example projects use the handmade / self-hosted [SC::Build](@ref page_build) system, and the
default workflow now builds them directly through the native backend.

## Command-line
- Build the test suite
    - **Windows**: `SC.bat build compile SCTest --config Debug`
    - **Posix**: `SC.sh build compile SCTest --config Debug`
- Run focused build-system coverage
    - **Windows**: `SC.bat build run SCBuildTest --config Debug`
    - **Posix**: `SC.sh build run SCBuildTest --config Debug`

@note Check the [Tools](@ref page_tools) page for more details on invoking `SC.sh build`.

## VSCode / IDE project generation

Under VSCode select `Tasks: Run Task` and choose:
- `Generate Projects`

Generated projects are still useful for Visual Studio, XCode, and Make workflows, but they are no longer required
before normal `build compile` / `build run` usage.

# Build the test suite

## Command-line

Intel Machines
- **Windows**: `SC.bat build compile SCTest --config Debug --arch intel64`
- **Posix**: `SC.sh build compile SCTest --config Debug --arch intel64`

Arm Machines
- **Windows**: `SC.bat build compile SCTest --config Debug --arch arm64`
- **Posix**: `SC.sh build compile SCTest --config Debug --arch arm64`

## Command-line Native Backend

- **macOS / Linux**: `./SC.sh build compile SCTest --config Debug`
- **macOS / Linux**: `./SC.sh build compile SCTest --config Debug --arch arm64 --verbose`
- **macOS / Linux**: `./SC.sh build run SCBuildTest --config Debug`
- **Windows**: `SC.bat build compile SCTest --config Debug`
- **Windows**: `SC.bat build run SCBuildTest --config Debug`

@note The native backend currently supports direct host builds on macOS, Linux, and Windows. Windows sysroot selection is
not implemented yet, and the exact toolchain combinations available still depend on the installed host tools.
@note Native `compile` and `run` commands also accept `--output quiet|normal|verbose`, or the `--quiet`, `--normal`,
and `--verbose` shortcuts. Legacy positional syntax is still accepted for compatibility.

## Visual Studio 2022
- Generate projects first: `SC.bat build configure`
- Open `_Build/_Projects/VisualStudio2022/SCWorkspace/SCWorkspace.sln`
- Build the default configuration target (or another one you prefer)

## XCode
- Generate projects first: `./SC.sh build configure`
- Open `_Build/_Projects/XCode/SCWorkspace/SCWorkspace.xcworkspace`
- Build the default configuration target (or another one you prefer)

## Makefile
- Generate projects first: `./SC.sh build configure`
- Linux: `cd _Build/_Projects/Make/SCWorkspace/linux`
- macOS: `cd _Build/_Projects/Make/SCWorkspace/apple`
- Build Debug: `make -j SCTest`
- Build Release: `make -j SCTest CONFIG=Release`

## VScode on macOS
Under VSCode select `Tasks: Run Task` and choose an appropriate targets like:
- `Build SCTest Debug intel64` [1]
- `Build SCTest Debug arm64` [1]
- `Build SCTest Release intel64` [1]
- `Build SCTest Release arm64` [1]

@note
[1] Uses `make` and `c++` command (can be switched to be `clang` or `gcc`). Builds only current host architecture (`arm64` or `x86_64`). Still needs XCode installed for the sysroot.  

## VSCode on Linux
Under VSCode select `Tasks: Run Task` and choose an appropriate targets like:
- `Build SCTest Debug intel64` [1]
- `Build SCTest Debug arm64` [1]
- `Build SCTest Release intel64` [1]
- `Build SCTest Release arm64` [1]

@note
[1] Uses `make` and `c++` commands. Builds only current host architecture (`arm64` or `x86_64`).

## VSCode on Windows
Under VSCode select `Tasks: Run Task` and choose an appropriate targets like:
- `Build SCTest Debug intel64` [1]
- `Build SCTest Debug arm64` [1]
- `Build SCTest Release intel64` [1]
- `Build SCTest Release arm64` [1]

@note
[1] Needs `Visual Studio 2022` installed

# Run the tests
Executables will be at `_Build/_Outputs/${platform}-${arch}-${build}-${compiler}-${config}/${EXAMPLE_NAME}`.  
For example assuming host to be ARM64 Linux, compiling with the native backend through Clang in Debug will cause the folder to be `_Build/_Outputs/linux-arm64-Native-clang-Debug/SCTest`.


# Debug the tests

## Visual Studio or XCode

Running the default target should work out of the box, as paths are already generated accordingly.

## VSCode on macOS or Linux

Select one of the appropriate `Run and Debug` configuration like:

- `SCTest intel64 [apple] (gdb)` [1]
- `SCTest intel64 [apple] (lldb)` [2]
- `SCTest arm64 [apple] (gdb)` [1]
- `SCTest arm64 [apple] (lldb)` [2]
- `SCTest intel64 [linux] (gdb)` [1]
- `SCTest intel64 [linux] (lldb)` [2]
- `SCTest arm64 [linux] (gdb)` [1]
- `SCTest arm64 [linux] (lldb)` [2]

@note
[1] Uses `gdb` debugger. Implicitly invokes `Build SCTest Debug` to build the executable.  
[2] Uses `lldb` debugger. Implicitly invokes `Build SCTest Debug` to build the executable. You need `CodeLLDB` or similar extension installed in VSCode.

## VSCode on Windows

Select one of the appropriate `Run and Debug` configuration like:

- `SCTest intel64 [windows]` [1]
- `SCTest arm64 [windows]` [1]

@note
[1] Needs `Visual Studio 2022` installed

# Build the documentation

The `SC-build.cpp` from [SC::Tool](@ref page_tools) downloads Doxygen and some customization themes and then builds the documentation in `_Build/_Documentation`.

## Command-line
- **Windows**: `SC.bat build documentation`
- **Posix**: `SC.sh build documentation`

## VSCode
Under VSCode select `Tasks: Run Task` and choose:
- `Build Documentation`
