@page page_build SC::Build

`SC::Build` describes a C++ build in C++ and can either execute it directly or generate projects for another build
system. This guide explains that model and the shortest useful workflow.

[TOC]

# The build model

An `SC-build.cpp` file is an executable build description. The repository launcher compiles it when needed, asks it to
create a graph of workspaces, projects, configurations, and files, then hands that graph to a backend.

```text
SC.sh / SC.bat
      |
      v
compile SC-build.cpp when it changed
      |
      v
create the project graph
      |
      +----> Native backend ----> compile and link now
      |
      +----> Generated backend -> Xcode, Visual Studio, or Make projects
```

This is why changing build logic does not require a separate scripting language or installed package manager. The build
description uses the same compiler, result handling, strings, paths, and process APIs as the rest of Sane C++.

# Build and run a target

From the SaneCppLibraries repository root, compile a target first and then run the produced executable:

```bash
./SC.sh build compile SaneHttpGet Debug
./SC.sh build run SaneHttpGet Debug
```

Use `SC.bat` on Windows. `build run` brings stale target inputs up to date before launching the executable. Contributor
and test workflows still use an explicit compile step first because it makes build failures and program failures easy to
distinguish.

The native backend is the default on macOS, Linux, and Windows. It writes intermediates and outputs below `_Build`
without requiring generated project files.

# Read a build definition

A small external definition has three jobs: name the target, add its source, and add Sane C++ Libraries.

```cpp
#include "SaneCppBuild.h"

SC::Result SC::Build::configure(Definition& definition, const Parameters& parameters)
{
    Project project = {"MyProgram"};
    project.setRootDirectory(parameters.directories.projectDirectory.view());
    SC_TRY(project.addFiles("Source", "*.cpp"));
    SC_TRY(addSaneCppLibraries(project, parameters));
    return definition.addProject(move(project));
}
```

The important types form a hierarchy:

- `Definition` owns the complete build graph.
- `Workspace` groups related projects. A simple definition can use the implicit default workspace.
- `Project` names an output and collects source files, include paths, dependencies, and link settings.
- `Configuration` changes settings for builds such as Debug and Release.

Settings can be attached at project, configuration, or file-group level. Start at project level and move a setting
deeper only when a target genuinely needs a different value.

# Native and generated workflows

Use the native backend for normal command-line development:

```bash
./SC.sh build compile SCTest Debug
```

Generate projects when the project file is itself useful, for example to work inside Xcode or Visual Studio:

```bash
./SC.sh build configure SCTest
```

Generated files live under `_Build/_Projects`. Native and generated outputs live under `_Build/_Outputs`, separated by
platform, architecture, backend, compiler, and configuration. These directories are disposable build products.

# Choose the integration size

`addSaneCppLibraries(project, parameters)` adds the unity-build `SC.cpp` form. It is the simplest option for most
programs because the library set becomes one compilation unit.

To compile the individual library sources instead:

```cpp
SC_TRY(addSaneCppLibraries(project, parameters, Libraries::Multiple));
```

Both forms add the public `Includes` directory, so target sources include library entry points such as
`SaneCppStrings.h` rather than repository-internal paths.

# Cross builds are three decisions

Cross compilation is easier to reason about when three concerns stay separate:

1. `--target` selects the destination platform and ABI profile.
2. `--toolchain` selects the compiler family when the profile does not already imply one.
3. `--runner` selects how a foreign executable is launched, if it can be launched on the host.

For example:

```bash
./SC.sh build compile SaneHttpGet Debug --target windows-gnu-x86_64
./SC.sh build run SaneHttpGet Debug --target windows-gnu-x86_64 --runner auto
```

Package-managed compilers, sysroots, and runners are installed through [SC::Package](@ref page_tools). Not every host
and target pair has the same build or run support. Use the command help for the current matrix:

```bash
./SC.sh build compile --help
./SC.sh build run --help
```

# Continue from here

- Use [SC::Build (External use)](@ref page_build_external) to adopt the build system outside this repository.
- Use [Building (Contributor)](@ref page_building_contributor) for the repository validation loop.
- Read `Tools/SC-build/Build.h` when you need the exact public API.
- Read `Tools/SC-build.cpp` for the build graph used by this repository.
