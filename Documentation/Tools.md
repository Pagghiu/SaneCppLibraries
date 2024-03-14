@page page_tools Tools

[TOC]

`SC::Tools` are small and self contained Sane C++ cross platform programs, that are compiled to host executable format on demand and run.  
They typically automate some repository action (like building or formatting code) with imperative Sane C++ code, instead of using a shell or scripting language.  
They use Sane C++ Libraries for FileSystem, Process / Shell or Async IO to fullfill their needs.  

As they're just regular programs, their functionality is not limited and entirely defined by the developer.  
And again, as they're regular programs, they can be easily debugged.

@note `SC::Tools` are compiled on the fly when needed, so they require a working host compiler to be available in system path.

# How does it work

The bootstrap batch (`SC.bat`) / bash (`SC.sh`) shell scripts are doing the following:  
- Two `.cpp` files are compiled with a single `clang` / `g++` / `cl.exe` invocation:
    - `Tools/Tools.cpp` file 
        - Includes the `SC.cpp` unity build file
        - Defines `main(argc, argv)`
    - `SC-$TOOL.cpp` file implementing the tool functionality
- The resulting executable is run
- All arguments passed to the bootstrap shell scripts are passed to the tool
- The first argument is the main `$ACTION` by convention

@warning Currently there is no mechanism to check if a command must be rebuilt.  
To rebuild a tool, invoke it without any option (for example `./SC.sh build`).  
This issue will be resolved in the future.

# Invoking a tool

All tools are invoked with the `SC.sh` or `SC.bat` bootstrap script that is located in the root of the repo.

To invoke you call it with the name of the tool and some parameters.  
For Example: 

```
./SC.sh build configure
```

or (on Windows)

```
SC.bat build compile
```

# Tools
Current tools that exist in the Sane C++ Repo

## SC-build.cpp

`SC-build` configures (generating projects) and compiles Sane C++ repository projects.

### Actions:

- `configure`: Configure (generates) the projects into `_Build/_Projects`
- `compile`: Compiles all projects in `_Build/_Projects`

### Examples:
Configure project, generating them:
```
./SC.sh build configure
```

Build all projects
```
./SC.sh build configure
```
