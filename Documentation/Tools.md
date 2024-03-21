@page page_tools Tools

[TOC]

`SC::Tools` are self contained Sane C++ cross platform _scripts_ compiled on the fly to be executed.  

They leverage the growing shell, system and network programming capabilities of Sane C++ Libraries, by just including the `SC.cpp` unity build file, that has no third party dependencies (see [Building (Contributor)](@ref page_building_contributor)).

# Reasons

`SC::Tools` has been created for the following reasons:  

- Enjoy the coolness of writing _C++ scripts_
- Do shell automation / development tools or similar tasks in a real programming language
- Allow C++ programmers to use regular C++ IDE / Debuggers when writing _automation / shell scripts_
- Use Sane C++ Libraries for real tasks to improve them and fix bugs:
    - Example: `Tools/SC-package.cpp` uses SC::Process library to download some third party binary 
    - Example: `Tools/SC-package.cpp` uses SC::Hashing library to check downloads MD5 hash
    - Example: `Tools/SC-format.cpp` uses SC::FileSystemIterator library to find all files to format in the repo
- Create portable scripts that can be written once and run (or be debugged) on all platforms
- Avoid introducing additional dependencies
- Keep the percentage of C++ code in the repo as high as possible (as a consequence of the above)

# Usage

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

@note `SC::Tools` are just regular programs being compiled on the fly when needed, so they require a working host compiler to be available in system path. This limitation could be removed if needed, as described in the Roadmap section.  


# Tools
This is the list of tools that currently exist in the Sane C++ repository.

## SC-build.cpp

`SC-build` configures (generating projects) and compiles Sane C++ repository projects.

### Actions

- `configure`: Configure (generates) the projects into `_Build/_Projects`
- `compile`: Compiles all projects in `_Build/_Projects`

### Examples
Configure project, generating them:
```
./SC.sh build configure
```

Build all projects
```
./SC.sh build compile
```

## SC-package.cpp

`SC-package` downloads third party tools needed for Sane C++ development (example: `clang-format`).  
Proper OS / Architecture combination is selected (Windows/Linux/Mac and Intel/ARM) and all downloaded files (_packages_) `MD5` hash is checked for correctness.  
_Packages_ are placed and extracted in `_Build/_Packages` and once extracted, they are symlinked in `_Build/Tools`.  

@note Directory naming has been chosen to avoid clashes when mounting the same working copy folder in multiple concurrent virtual machines with different Operating Systems / architectures.  
This happens during regular development, where new code is frequently tested in parallel on macOS, Windows and Linux before even committing it and pushing it to the CI system.

### Actions

- `install`: Downloads requires tools (LLVM / 7zip)

### Examples

```
./SC.sh package install
```

### Packages
These are the packages that are currently downloaded and extracted / symlinked by `SC-package.cpp`:

- `LLVM 15`: Downloads LLVM from the official github repository
- `7zip`: 7zip executable (needed to decompress LLVM installer on Windows)
- `7zr.exe`: 7Zip console executable (needed to decompress 7zip installer on Windows)

## SC-format.cpp

`SC-format` tool formats or checks formatting for all files in Sane C++ repository

### Actions
- `execute`: Formats all source files in the repository in-place
- `check`: Does a dry-run format of all source files. Returns error on unformatted files (used by the CI)

### Examples

Format all source files in the repository
```
./SC.sh format execute
```

Check if all source files in the repository are properly formatted
```
SC.bat format check
```

# How does it work

The bootstrap batch (`SC.bat`) / bash (`SC.sh`) shell scripts are doing the following:  
- Two `.cpp` files are compiled with a single `clang` / `g++` / `cl.exe` invocation
    - `Tools/Tools.cpp` file 
        - Includes the `SC.cpp` unity build file, defining `main(argc, argv)`
    - `SC-$TOOL.cpp` file
        - Implements the specific tool functionality
- The first argument after `$TOOL` name is the main `$ACTION` by convention
- `$ACTION` and all `$PARAM_X` arguments passed to the bootstrap shell scripts are forwarded to the tool
- The resulting executable is run with the forwarded arguments

Example:  
`SC.sh $TOOL $ACTION $PARAM_1 $PARAM_2 ... $PARAM_N`

@warning Currently there is no mechanism to check if a command must be rebuilt.  
To rebuild a tool, invoke it without any option (for example `./SC.sh build`).  
This issue will be resolved in the future, by supporting precise out-of-date source level dependency tracking.

# Roadmap

- Rebuild the executable automatically when source script (or SC.cpp) changes (dependency tracking)
- Generate projects to debug the programs easily
- Investigate better way of expressing the dependencies chain between scripts
- Do not require to have a C++ toolchain already installed on the system [*]

[*] Running the Sane C++ Tools scripts on a machine without a pre-installed compiler could be implemented by shipping a pre-compiled `SC-package.cpp` (that is already capable of downloading `clang` on all platforms) or by adding to the bootstrap script ability to download a suitable compiler (and sysroot).

