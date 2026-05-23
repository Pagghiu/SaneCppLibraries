@page page_faq FAQ

[TOC]

# What is the default Standard C++ Library mode?
By default SC allows C and C++ standard headers, so normal C++ projects can include the libraries without an SC-specific
stdlib opt-in macro. SC library code still avoids STL containers, exceptions, RTTI, hidden allocations, and C++ standard
library features that force the C++ runtime when practical. SC-build defaults to the normal C++ integration model:
standard headers are available and the C++ standard-library runtime may be linked.

The old `SC_COMPILER_ENABLE_STD_CPP` macro has been removed. Defining it now stops compilation with a migration error.

# Can I disable Standard C++ Library headers?
Yes. Define `SC_INCLUDE_STD_CPP=0` and, if you are not using SC-build, add the matching compiler flags for your
toolchain:
- *XCode / Clang / GCC 13+*:
    - Add **--nostdinc++**

With SC-build, set `project.files.compile.includeStdCpp = false` on the target or
`configuration.compile.includeStdCpp = false` on a specific configuration. For Sane C++ targets this also emits
`SC_INCLUDE_STD_CPP=0`.

To avoid C++ runtime linkage without disabling headers, set `project.link.linkStdCpp = false`; Sane C++ targets should
also set `project.saneCpp.provideCppRuntimeShims = true` unless those runtime ABI symbols are provided elsewhere. If you
are not using SC-build, define `SC_PROVIDE_CPP_RUNTIME_SHIMS=1` and pass **--nostdlib++** manually on Clang / GCC 13+.

# Can standard C++ headers be used without linking the C++ runtime?
For constrained SC-style code, yes on the toolchains currently measured by `Support/Scripts/CheckStdCppHeaderNoLink.*`.
This is a toolchain-tested compatibility claim, not a blanket C++ language guarantee.

| Platform / compiler | Header probe | C++ runtime dependency |
|:--|:--|:--|
| macOS Apple clang 17 | `<coroutine>` | Not linked in the constrained probe |
| Ubuntu clang 18 | `<coroutine>` | Not linked in the constrained probe |
| Ubuntu GCC 13.3 | `<coroutine>` | Not linked in the constrained probe |
| Windows MSVC 14.44 | `<coroutine>` | Not linked in the constrained probe |
| Windows clang-cl | `<coroutine>` | Not linked in the constrained probe |

Run `Support/Scripts/CheckStdCppHeaderNoLink.sh` on macOS/Linux or
`Support\Scripts\CheckStdCppHeaderNoLink.bat` from a Visual Studio developer prompt on Windows to refresh the local
evidence.

# Can I benchmark normal mode versus strict mode?
Yes. `Support/Scripts/BenchmarkStdCppMode.sh` builds configurable normal and strict targets for multiple iterations and
reports build timing, executable size, section sizes, and linked runtime dependencies. The benchmark is informational and
is intentionally not a CI threshold.

# Can I disable Exceptions and RTTI?
Yes, if you disable the Standard C++ Library, you can also disable Exceptions and RTTI
- *XCode / Clang / GCC*: 
    - Add **-fno-exceptions**
    - Add **-fno-rtti**
- *Visual Studio / MSVC / ClangCL*
    - Set **Enable C++ Exceptions** to **No**
    - Set **Enable Runtime Type Information** set to **No** (**GR-**)

# Can I use STL types like std::string or std::vector or my own containers?
Yes you can, see the example at `Tests/InteropSTL`.  
You don't have to use the STL then, just any container library that you happen to like.   
The integration is efficient in the sense that for example String format or File Read will write data directly to the memory provided by your supplied string and/or container.  
The abstraction tries to be efficient issuing an indirect function pointer call only if an actual allocation is needed (after entirely using the provided initial capacity).

# Does the library have debug visualizers?

Yes, this library contains debug visualizers for:
- SC::Array
- SC::Buffer
- SC::SmallBuffer
- SC::SmallString
- SC::SmallVector
- SC::String
- SC::StringSpan
- SC::StringView
- SC::Vector

Targeting:

- `lldb` (`Support/DebugVisualizers/GDB/SCGDB.py`)
- `gdb` (`Support/DebugVisualizers/LLDB/SCLLDB.py`)
- `VS Debugger` (`Support/DebugVisualizers/MSVC/SCMSVC.natvis`). 

Usage:

- [Build](@ref page_build) is setting them up automatically for XCode and Visual Studio projects.
- Starting the VSCode `launch.json` configurations for lldb and gdb should will load them in your debugger session.
- You can manually `source` or add them to your project or build system

# What plans for ABI / API stability?

There are no plans to provide ABI stability.

Each library declares its own API stability, but as the project is very young, expect breaking changes for now.  
At some point API will stabilize naturally and it will be made explicit for each library.
