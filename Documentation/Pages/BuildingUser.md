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

- Add the `SC.cpp` unity build file to your build system of choice (if any!).
- Headers in `Libraries/**SomeLibrary**/*.h` are public.  
- Headers in `Libraries/**SomeLibrary**/[Internal | Tests]` are **NOT** public.

@note
Libraries can be used from normal C++ projects by default. SC library code still avoids STL containers, exceptions,
RTTI, and hidden allocations, but public integration no longer requires an opt-in macro for standard C/C++ headers.
SC-build also defaults to normal C++ runtime linkage; set `project.link.linkStdCpp = false` only when you intentionally
want to avoid linking the C++ standard-library runtime.
If you want the stricter historical no-stdlib mode, define `SC_INCLUDE_STD_CPP=0` and
`SC_PROVIDE_CPP_RUNTIME_SHIMS=1`, then check
[Disable the C++ standard library](@ref page_faq).

## macOS
- Add `SC.cpp` (located in project root) to your build
- Include any public header located at `Libraries/**SomeLibrary**/SomeHeader.h`
- Link:
    - `CoreFoundation.framework`
    - `CoreServices.framework`

## Linux
- Add `SC.cpp` (located in project root) to your build
- Include any public header located at `Libraries/**SomeLibrary**/SomeHeader.h`
- Link:
    - `libdl` (`-ldl`) 
    - `libpthread` (`-lpthread`)

## Windows
- Add `SC.cpp` (located in project root) to your build
- Include any public header located at `Libraries/**SomeLibrary**/SomeHeader.h`

@note on Windows the following libraries are already implicitly linked through `#pragma comment(lib, ...)`
- `Ws2_32.lib`
- `ntdll.lib`
- `Rstrtmgr.lib`
- `DbgHelp.lib`

If you use the [Plugin](@ref library_plugin) library without `SC::Build`, make sure the host executable defines
`SC_EXPORT_LIBRARY_<LIBRARY>=1` for each Sane C++ library that must be visible to plugins; on Linux also add
`-rdynamic` to the host executable link step.

# Single Files Amalgamation

Sane C++ Libraries can be consumed also as single-file amalgamated files.

To generate them in `_Build/_SingleFileLibraries` run:

```sh
python3 Support/SingleFileLibs/python/amalgamate_single_file_libs.py
```

alternatively if you have node.js installed run:

```sh
node Support/SingleFileLibs/javascript/cli.js --repo-root . --ref HEAD --all --out _Build/_SingleFileLibrariesJS
```

As a third alternative you can assemble them in the browser at the [SingleFileLibs](@ref page_single_file_libs) page
