@page library_build Build

@brief 🟥 Minimal build system where builds are described in C++

Build uses C++ to imperatively describe a sequence of build operations.  

[TOC]

# Features

- Describe builds in a cpp file
- Generate XCode 14 console Projects
- Generate Visual Studio 2022 console Projects

# Status
🟥 Draft  
Build currently used to generate test projects of this repository but still not mature enough to be used for anything else.

# Description

Build C++ files (named by convention `SCBuild.cpp`) are compiled the fly and they generate project files for existing build systems.  
In the future the plan is to allow also the system to run standalone directly invoking compilers to produce libraries and executables.

This is for example the `SCBuild.cpp` file for the test suite:
\include SCBuild.cpp

The abstraction is described by the following (top-down) hierarchy:

| Class                         | Description                           |
|:------------------------------|---------------------------------------|
| SC::Build::Definition         | @copybrief SC::Build::Definition      |
| SC::Build::Workspace          | @copybrief SC::Build::Workspace       |
| SC::Build::Project            | @copybrief SC::Build::Project         |
| SC::Build::Configuration      | @copybrief SC::Build::Configuration   |

Some additional types allow describing detailed properties of the build:

| Class                         | Description                           |
|:------------------------------|---------------------------------------|
| SC::Build::Platform           | @copybrief SC::Build::Platform        |
| SC::Build::Architecture       | @copybrief SC::Build::Architecture    |
| SC::Build::Generator          | @copybrief SC::Build::Generator       |
| SC::Build::Optimization       | @copybrief SC::Build::Optimization    |
| SC::Build::Compile            | @copybrief SC::Build::Compile         |
| SC::Build::CompileFlags       | @copybrief SC::Build::CompileFlags    |
| SC::Build::Link               | @copybrief SC::Build::Link            |
| SC::Build::LinkFlags          | @copybrief SC::Build::LinkFlags       |


# Architecture

- User adds a `SCBuild.cpp` files to his library
- `SCBuild.cpp` describes the SC::Build::Definition using imperative C++ code
    - Define at least one SC::Build::Workspace
    - Define at least one SC::Build::Project 
    - Define at least one SC::Build::Configuration
- `SCBuild.cpp` is compiled to an executable with a single `clang` or `cl.exe` invocation
- The unity build file `SC.cpp` is linked together in the same invocation
    - Provides all needed Platform abstraction features to enumerate files and directories, launch processes etc.
- The generated executable is run as specified in SC::Build::Parameters
- SC::Build::Parameters specifies a combination of:
    - SC::Build::Platform @copybrief SC::Build::Platform
    - SC::Build::Configuration @copybrief SC::Build::Configuration
    - SC::Build::Generator @copybrief SC::Build::Generator
- `SCBuild.cpp` calls SC::Build::ConfigurePresets::generateAllPlatforms or manually invokes SC::Build::Definition::generate
- Debugging the build script means just debugging a regular C++ executable

So far the entire build configuration is created in C++ but each invocation with a different set of "build parameters" it's building a data structure that is free of conditionals, as they've been evaluated by the imperative code.
Such "post-configure" build settings could be serialized to JSON (or using binary [Serialization](@ref library_serialization_binary)) or to any other declarative format if needed.  

# Roadmap

🟨 MVP Features:
- Build non-console type projects
- Build artifacts invoking with direct compiler invocation (bypass XCode / VisualStudio build systems)

🟩 Usable Features:
- Describe more project types (dynamic libraries, static libraries etc.)
- Describe dependencies between targets
- Allow nested builds (where the root script should compile child scripts found down the path)

🟦 Complete Features:
- Generate one click ready to debug projects for build itself
- Describe very complex builds (this will be probably only doable porting makefiles of some external library)

💡 Unplanned Features:
- Compile scripts to WASM so that they can't run arbitrary code