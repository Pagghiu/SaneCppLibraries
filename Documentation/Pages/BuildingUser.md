@page page_building_user Building (User)

[TOC]

This is the guide to follow if you just want to use the libraries in your project, without running the test suite.  
If you want to contribute or run the test suite, check the [Building (Contributor)](@ref page_building_contributor) page.

# Principles
- Libraries always meant to be compiled together with your existing project
- All libraries do not require any build system (you're free to use your favorite!)
- For the above reasons you don't really _build_ the libraries
- You _add_ or _use_ them to your project just as another source file

# Use in your project

- Add a single file named `SC.cpp`
- Headers in `Libraries/**SomeLibrary**/*.h` are public.  
- Headers in `Libraries/**SomeLibrary**/[Internal | Tests]` are **NOT** public.

@note
Libraries assume you're not using the Standard C++ Library to ensure peak C++ sanity in your project.  
If you really need you can:
    - `#define` `SC_COMPILER_ENABLE_STD_CPP=1` if you really need Standard C++ Library
    - But remember you can always [Disable the standard C++ library](@ref page_faq) with a few flags

## macOS
- Add `SC.cpp` (located at `Bindings/cpp`) to your build
- Include any public header located at `Libraries/**SomeLibrary**/SomeHeader.h`
- Link:
    - `CoreFoundation.framework`
    - `CoreServices.framework`

## Linux
- Add `SC.cpp` (located at `Bindings/cpp`) to your build
- Include any public header located at `Libraries/**SomeLibrary**/SomeHeader.h`
- Link:
    - `libdl` (`-ldl`) 
    - `libpthread` (`-lpthread`)

## Windows
- Add `SC.cpp` (located at `Bindings/cpp`) to your build
- Include any public header located at `Libraries/**SomeLibrary**/SomeHeader.h`

@note on Windows the following libraries are already implicitly linked through `#pragma comment(lib, ...)`
- `Ws2_32.lib`
- `ntdll.lib`
- `Rstrtmgr.lib`
- `shlwapi.lib`

