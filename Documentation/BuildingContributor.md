@page page_building_contributor Building (Contributor)

Follow this guide if you're interested in building the library to contribute (check [CONTRIBUTING.md](https://github.com/Pagghiu/SaneCppLibraries/blob/main/CONTRIBUTING.md)!) or you're simply curious to run the test suite.

[TOC]

### Generate test projects

The test suite uses the handmade / self-hosted [SC::Build](@ref library_build) system, that describes builds in C++

- Generate test project
    - **Linux**: `SCBuild.sh`
    - **Windows**: `SCBuild.bat`
    - **macOS**: `SCBuild.command`
- Open generated projects (in `_Build/Projects`). 

@note These batch / bash scripts are doing the following:  
- Two `.cpp` files are compiled with a single `clang` / `g++` / `cl.exe` invocation:
    - `SC.cpp` unity build file encompassing the entire library
    - `SCBuild.cpp` file defining the build
- The resulting executable is then run to generate projects.

### Build the test suite

#### Visual Studio 2022
- Open `_Build/Projects/VisualStudio2022/SCTest.sln` 
- Build the default configuration target (or another one you prefer)

#### XCode
- Open `_Build/Projects/XCode/SCTest.xcodeproj/project.xcworkspace` 
- Build the default configuration target (or another one you prefer)

#### VScode on macOS
Under VSCode select `Tasks: Run Task` and choose an appropriate targets like:
- `Build SCTest Debug` [1]
- `Build SCTest Release` [1]
- `Build SCTest XCode Debug` [2]
- `Build SCTest XCode Release` [2]

@note
[1] Needs only `make` and `c++` command (can be switched to be `clang` or `gcc`)  
[2] Needs XCode installed

#### VSCode on Linux
Under VSCode select `Tasks: Run Task` and choose an appropriate targets like:
- `Build SCTest Debug` [1]
- `Build SCTest Release` [1]

@note
[1] Needs only `make` and `c++`

#### VSCode on Windows
Under VSCode select `Tasks: Run Task` and choose an appropriate targets like:
- `Build SCTest Debug` [1]
- `Build SCTest Debug ARM64` [1]

@note
[1] Needs `Visual Studio 2022` installed

### Debug the tests

#### Visual Studio or XCode

Running the default target should work out of the box, as paths are already generated accordingly.

#### VSCode on macOS or Linux

Select one of the appropriate `Run and Debug` configuration like:

- `SCTest [posix] (gdb)` [1]
- `SCTest [posix] (lldb)` [2]

@note
[1] Uses `gdb` debugger. Implicitly invokes `Build SCTest Debug` to build the executable.  
[2] Uses `lldb` debugger. Implicitly invokes `Build SCTest Debug` to build the executable. You need `CodeLLDB` or similar extension installed in VSCode.

#### VSCode on Windows

Select one of the appropriate `Run and Debug` configuration like:

- `SCTest x64 [win] (vsdbg)` [1]
- `SCTest ARM64 [win] (vsdbg)` [1]

@note
[1] Needs `Visual Studio 2022` installed
