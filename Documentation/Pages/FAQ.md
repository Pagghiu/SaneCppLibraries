@page page_faq FAQ

[TOC]

# Can I disable Standard C++ Library?
Yes, if you're satisfied with the C++ standard library alternatives included in this SC and do not plan to use C++ Standard Library in your project:
- *XCode / Clang / GCC 13+*: 
    - Add **--nostdlib++**
    - Add **--nostdinc++**
- *GCC < 13*:
    - Not supported (missing **--nostdlib++**)

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

