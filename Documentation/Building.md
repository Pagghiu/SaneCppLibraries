## Building

[TOC]
### Add to your project

- The library is only delivered in source form, and not as a binary.
- To add it in your existing program you will have to add a single file named `SC.cpp`
- Headers in `Libraries/**SomeLibrary**/*.h` are public.  
- Headers in `Libraries/**SomeLibrary**/[Internal | Tests]` are **NOT** public.
- You can customize the build changing [Preprocessor Options](#autotoc_md5) 

#### Windows
- Add `SC.cpp` (located at `Bindings/cpp`) to your build
- Include any public header located at `Libraries/**SomeLibrary**/SomeHeader.h`

#### macOS
- Add `SC.cpp` (located at `Bindings/cpp`) to your build
- Include any public header located at `Libraries/**SomeLibrary**/SomeHeader.h`
- Link:
    - `CoreFoundation.framework`
    - `CoreServices.framework`

#### Linux
- Add `SC.cpp` (located at `Bindings/cpp`) to your build
- Include any public header located at `Libraries/**SomeLibrary**/SomeHeader.h`
- Link:
    - `libdl` (`-ldl`) 
    - `libpthread` (`-lpthread`)

@note on Windows the following libraries are already implicitly linked through `#pragma comment(lib, ...)`
- `Ws2_32.lib`
- `ntdll.lib`
- `Rstrtmgr.lib`
- `shlwapi.lib`

### Preprocessor Options

#### SC_COMPILER_ENABLE_STD_CPP=1

- Allows using the Standard C++ Library.  
- You can forcefully [Disable the standard C++ library](@ref page_faq) in your project if you like. 

### Generate test projects

- Generate test project
    - **Linux**: `SCBuild.sh`
    - **Windows**: `SCBuild.bat`
    - **macOS**: `SCBuild.command`
- Open generated projects (in `_Build/Projects`). 

Projects are generated using the self-hosted [Build](@ref library_build) system that allows describing projects using C++.  
- Two `.cpp` files are compiled with a single `clang` / `g++` / `cl.exe` invocation:
    - `SC.cpp` unity build file encompassing the entire library
    - `SCBuild.cpp` file defining the build
- The resulting executable is then run to generate projects.

### Build the tests

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

[1] Needs only `make` and `c++` command (can be switched to be `clang` or `gcc`)  
[2] Needs XCode installed

#### VScode on Linux
Under VSCode select `Tasks: Run Task` and choose an appropriate targets like:
- `Build SCTest Debug` [1]
- `Build SCTest Release` [1]

[1] Needs only `make` and `c++`

### Debug the tests

#### Visual Studio or XCode

Running the default target should work out of the box, as paths are already generated accordingly.

#### VSCode on macOS or Linux

Select any `Run and Debug` configuration like:

- `(gdb) Debug SCTest` [1]
- `(lldb) Debug SCTest` [2]

[1] Uses `gdb` debugger. Implicitly invokes `Build SCTest Debug` to build the executable.  
[2] Uses `lldb` debugger. Implicitly invokes `Build SCTest Debug` to build the executable.
