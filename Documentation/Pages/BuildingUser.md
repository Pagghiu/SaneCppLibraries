@page page_building_user Building (User)

Sane C++ libraries are source libraries: add the code you need to your program and compile it with the rest of your
sources. You do not install a prebuilt runtime or adopt a mandatory build system.

[TOC]

# Choose an integration form

The repository offers three forms of the same code. Pick the one that matches how much control your project needs.

| Form | Use it when | What you add |
|:--|:--|:--|
| Unity build | You want the simplest complete integration | `SC.cpp` plus public headers from `Includes/` |
| Separate sources | Your build needs per-library or per-file compilation | Selected files under `Libraries/` plus public headers |
| Amalgamated library | You want one standalone library header/source pair | A generated file from the amalgamation tool |

Start with `SC.cpp` unless you already know why separate compilation is useful. The public library entry points are
headers such as `SaneCppStrings.h` and `SaneCppFile.h`. Files below `Internal` and `Tests` are not public API.

# Add one library to an existing program

1. Add `SC.cpp` to the target's source files.
2. Add the repository `Includes` directory to the target's include paths.
3. Include the entry-point header for the library you use.
4. Add the small set of platform link dependencies required by your selected libraries.

For example:

```cpp
#include "SaneCppStrings.h"

int main()
{
    SC::Console console;
    console.print("{1} {0}!\n", "world", "Hello");
    return 0;
}
```

The [library catalog](@ref libraries) identifies each library's dependencies and standalone download. The
[Platforms](@ref page_platforms) guide describes supported operating systems.

# Use normal C++ around Sane C++

Normal projects may include C and C++ standard headers. Sane C++ library code still avoids STL containers, exceptions,
RTTI, and hidden allocation, but your application is free to use them.

The APIs are designed to accept views and caller-provided storage, so application containers can interoperate without
becoming library dependencies. For example, an application can expose writable storage from its own string type to a
file read or formatter.

Strict no-standard-library compilation is an optional pressure mode, not the default integration path. The [FAQ](@ref
page_faq) explains the compile and link settings when you intentionally need it.

# Account for platform libraries

The unity build selects platform implementations at compile time. Depending on the libraries used, a target may need:

- Apple system frameworks such as CoreFoundation or CoreServices;
- Linux system libraries such as `dl` and `pthread`;
- Windows system libraries, many of which are selected by the headers with `#pragma comment(lib, ...)`.

Start from the link settings used by the corresponding repository example or test rather than adding every possible
system library. This keeps a small consumer target honest about what it actually uses.

# Plugins need exported library symbols

When using [Plugin](@ref library_plugin) without `SC::Build`, define `SC_EXPORT_LIBRARY_<LIBRARY>=1` in the host for
each Sane C++ library that a plugin must resolve. Linux hosts also need exported executable symbols, normally through
`-rdynamic`.

# When to use SC::Build

Your existing build system remains a valid choice. Use [SC::Build](@ref page_build) when its C++ build descriptions,
native backend, cross-target profiles, or package-managed toolchains remove work from your project. The external-use
guide shows a minimal adoption that does not copy the Sane C++ repository layout.

For a single self-contained library, open the [Single File Amalgamation](@ref page_single_file_libs) tool instead.
