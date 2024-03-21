@page page_building_contributor Building (Contributor)

Follow this guide if you're interested in building the library to contribute (check [CONTRIBUTING.md](https://github.com/Pagghiu/SaneCppLibraries/blob/main/CONTRIBUTING.md)!) or you're simply curious to run the test suite.

[TOC]

### Generate test projects

The test suite uses the handmade / self-hosted [SC::Build](@ref library_build) system, that describes builds in C++

- Generate test project
    - **Windows**: `SC.bat build`
    - **Posix**: `SC.sh build`
- Open generated projects (in `_Build/_Projects`). 

@note Check the [Tools](@ref page_tools) page for more details on invoking `SC.sh build`.

### Build the test suite

#### Visual Studio 2022
- Open `_Build/_Projects/VisualStudio2022/SCTest.sln` 
- Build the default configuration target (or another one you prefer)

#### XCode
- Open `_Build/_Projects/XCode/SCTest.xcodeproj/project.xcworkspace` 
- Build the default configuration target (or another one you prefer)

#### VScode on macOS
Under VSCode select `Tasks: Run Task` and choose an appropriate targets like:
- `Build SCTest Debug intel64` [1]
- `Build SCTest Debug arm64` [1]
- `Build SCTest Release intel64` [1]
- `Build SCTest Release arm64` [1]

@note
[1] Uses `make` and `c++` command (can be switched to be `clang` or `gcc`). Builds only current host architecture (`arm64` or `x86_64`). Still needs XCode installed for the sysroot.  

#### VSCode on Linux
Under VSCode select `Tasks: Run Task` and choose an appropriate targets like:
- `Build SCTest Debug intel64` [1]
- `Build SCTest Debug arm64` [1]
- `Build SCTest Release intel64` [1]
- `Build SCTest Release arm64` [1]

@note
[1] Uses `make` and `c++` commands. Builds only current host architecture (`arm64` or `x86_64`).

#### VSCode on Windows
Under VSCode select `Tasks: Run Task` and choose an appropriate targets like:
- `Build SCTest Debug intel64` [1]
- `Build SCTest Debug arm64` [1]
- `Build SCTest Release intel64` [1]
- `Build SCTest Release arm64` [1]

@note
[1] Needs `Visual Studio 2022` installed

### Debug the tests

#### Visual Studio or XCode

Running the default target should work out of the box, as paths are already generated accordingly.

#### VSCode on macOS or Linux

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

#### VSCode on Windows

Select one of the appropriate `Run and Debug` configuration like:

- `SCTest intel64 [windows]` [1]
- `SCTest arm64 [windows]` [1]

@note
[1] Needs `Visual Studio 2022` installed
