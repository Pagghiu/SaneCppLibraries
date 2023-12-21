@page page_how_to How To

[TOC]

# Disable Standard C++ Library
If you're satisfied with the C++ standard library alternatives included in this SC and do not plan to use C++ Standard Library in your project:
- *XCode / Clang / GCC*: 
    - Add **--nostdlib**
    - Add **--nostdinc++**

# Disable Exceptions and RTTI
If you disable the Standard C++ Library, you can also disable Exceptions and RTTI
- *XCode / Clang / GCC*: 
    - Add **-fno-exceptions**
    - Add **-fno-rtti**
- *Visual Studio / MSVC / ClangCL*
    - Set **Enable C++ Exceptions** to **No**
    - Set **Enable Runtime Type Information** set to **No** (**GR-**)

# Use Debug Visualizers

This library contains debug visualizers for [Containers](@ref library_containers) and [Strings](@ref library_strings) for `clang` and `cl.exe` (Microsoft Compiler).
Using them will make your life easier.
[Build](@ref library_build) is setting them up automatically for XCode and Visual Studio projects.
