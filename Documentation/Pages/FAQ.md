@page page_faq FAQ

These answers clarify integration choices that are easy to confuse with the constraints followed by Sane C++ library
code itself.

[TOC]

# Can my application use the C++ standard library?

Yes. Normal integration allows C and C++ standard headers, and the application may use STL containers, exceptions, or
RTTI. The Sane C++ libraries avoid those facilities internally so their own ownership, failure, allocation, and runtime
costs stay explicit.

The old `SC_COMPILER_ENABLE_STD_CPP` opt-in macro no longer exists. Remove it rather than replacing it.

# How do application containers interoperate?

Many APIs accept views, spans, or growable-buffer interfaces instead of a concrete owning container. An application can
therefore expose storage from `std::string`, `std::vector`, or another container without making that type part of the
library API.

The useful boundary is storage and lifetime: the application owns the container; the library reads or writes the view
provided for the duration documented by the operation.

# What is strict no-standard-library mode?

Strict mode is an optional build-pressure configuration used to prove that a target does not depend on standard C++
headers or runtime support.

With SC::Build, disable standard C++ headers and runtime linkage on the project or a configuration:

```cpp
project.files.compile.includeStdCpp = false;
project.link.linkStdCpp = false;
project.saneCpp.provideCppRuntimeShims = true;
```

Without SC::Build, define `SC_INCLUDE_STD_CPP=0` and `SC_PROVIDE_CPP_RUNTIME_SHIMS=1`, then pass the equivalent compiler
and linker flags. On Clang and recent GCC these are commonly `-nostdinc++` and `-nostdlib++`.

Do not enable runtime shims while also linking the C++ runtime; both would provide the same ABI symbols.

# Can standard headers be used without linking the C++ runtime?

Sometimes. The repository probes a constrained `<coroutine>` use on its supported toolchains with
`Support/Scripts/CheckStdCppHeaderNoLink.sh` and the Windows `.bat` equivalent. That evidence applies to the tested code
shape and toolchain, not to arbitrary standard-library features.

Use `Support/Scripts/BenchmarkStdCppMode.sh` to compare normal and strict builds locally. It reports timing, executable
size, sections, and linked runtime dependencies; it is diagnostic evidence, not a CI performance threshold.

# Can exceptions and RTTI be disabled?

Yes. Sane C++ code does not require them. Use `-fno-exceptions -fno-rtti` with Clang or GCC, or disable the corresponding
MSVC project options. Your application and every linked dependency must follow compatible assumptions.

# Which debugger visualizers are available?

The repository includes visualizers for common arrays, buffers, strings, views, and vectors:

- LLDB: `Support/DebugVisualizers/LLDB/SCLLDB.py`
- GDB: `Support/DebugVisualizers/GDB/SCGDB.py`
- Visual Studio: `Support/DebugVisualizers/MSVC/SCMSVC.natvis`

Generated Xcode and Visual Studio projects configure the relevant support where possible. Other debugger setups can
load the file manually. If a visualizer stops matching a type layout, treat the C++ type as authoritative and update
the visualizer alongside it.

# Are API and ABI stable?

There is no project-wide ABI-stability promise. Each library records its maturity and may evolve independently. Public
API stability should be inferred from that library's documentation and release history, not from the age of the
repository as a whole.

Prefer source integration or a pinned revision when repeatability matters. A source-level pin makes API changes visible
at compile time and avoids assuming binary compatibility that the project does not claim.
