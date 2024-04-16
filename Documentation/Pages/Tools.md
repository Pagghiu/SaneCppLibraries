@page page_tools Tools

[TOC]

`SC::Tools` are self contained single C++ source files that are (automatically) compiled on the fly and linked to Sane C++ to be immediately executed.  

They leverage the growing shell, system and network programming capabilities of Sane C++ Libraries, by just including the `SC.cpp` unity build file, that has no third party dependencies (see [Building (Contributor)](@ref page_building_contributor)).  

Another way to look at them is just as small _C++ scripts_ for which you don't need to setup or maintain a build system, as long as you only use Sane C++ Libraries.

Check the [March 2024 Blog update post](https://pagghiu.github.io/site/blog/2024-03-27-SaneCppLibrariesUpdate.html) for more details.

@note Name is `SC::Tools` and not `SC::Scripts` because they're still just small programs.  
If the system will be generalized even more, maybe acquiring more advanced capabilities from [SC::Plugin](@ref library_plugin) and [SC::Build](@ref library_build) or sandboxing capabilities, this naming will be re-evaluated and/or changed.

# Reasons

`SC::Tools` has been created for the following reasons:  

- Enjoy the coolness of writing _C++ scripts_
- Create development tools and automate shell operations in a real programming language
- Allow C++ programmers to use regular C++ IDE / Debuggers when writing _automation / shell scripts_
- Use Sane C++ Libraries for real tasks to improve them and fix bugs:
    - Example: `Tools/SC-package.cpp` uses SC::Process library to download some third party binary 
    - Example: `Tools/SC-package.cpp` uses SC::Hashing library to check downloads MD5 hash
    - Example: `Tools/SC-format.cpp` uses SC::FileSystemIterator library to find all files to format in the repo
- Create portable scripts that can be written once and run (or be debugged) on all platforms
- Avoid introducing additional dependencies
- Keep the percentage of C++ code in the repo as high as possible (as a consequence of the above)

# Invoking built-in Tools

All built-in tools are invoked with the `SC.sh` or `SC.bat` bootstrap script that is located in the root of the repo.

Such script must be called with the name of the tool and some parameters.  

For Example, invoking the `Tools\SC-build.cpp` tool with `configure` action: 

```
./SC.sh build configure
```

or (on Windows)

```
SC.bat build compile
```

@note `SC::Tools` are just regular programs being compiled on the fly when needed, so they require a working host compiler to be available in system path. This limitation could be removed if needed, as described in the Roadmap section.  

# Invoking custom tools

Tools can be automatically compiled and run by just passing its full path to the `SC` bootstrap.

Given the following custom tool at `myToolDirectory/TestScript.cpp`

```cpp
#include "Libraries/Strings/Console.h"
#include "Tools/Tools.h"
namespace SC
{
namespace Tools
{
StringView Tool::getToolName() { return "Test"; }
StringView Tool::getDefaultAction() { return "test"; }
Result     Tool::runTool(Tool::Arguments& toolArgs)
{
    toolArgs.console.printLine("Hey This is the output from My Tool!!!");
    toolArgs.console.printLine(toolArgs.action);
    for(auto arg : toolArgs.arguments)
        toolArgs.console.printLine(arg);
    return Result(true);
}
} // namespace Tools
} // namespace SC


```

This is how to invoke the tool with `action` and some parameters
```
./SC.sh myToolDirectory/TestScript.cpp myAction param1 param2 ..
```

or (on Windows):

```
SC.bat myToolDirectory\TestScript.cpp myAction param1 param2 ...
```

Possible output (Posix):
```
 ~/Developer/Projects/SC > ./SC.sh TestScript.cpp myAction param1 param2
Starting TestScript
Test "myAction" started
librarySource    = "/Users/stefano/Developer/Projects/SC"
toolSource       = "/Users/stefano/Developer/Projects/SC/Tools"
outputs          = "/Users/stefano/Developer/Projects/SC/_Build"
Hey This is the output from My Tool!!!
myAction
param1
param2
Test "myAction" finished (took 1 ms)
```

This is the list of tools that currently exist in the Sane C++ repository.

# SC-build.cpp

`SC-build` configures (generating projects) and compiles Sane C++ repository projects using [SC::Build](@ref library_build).

## Actions

- `configure`: Configure (generates) the projects into `_Build/_Projects`
- `compile`: Compiles all projects into `_Build/_Projects`
- `documentation`: Builds the documentation into `_Build/_Documentation`
- `coverage`: Builds the clang source coverage into `_Build/_Coverage`

## Examples
Configure project, generating them:
```
./SC.sh build configure
```
Possible Output:

```
 ~/Developer/Projects/SC > ./SC.sh build configure
Compiling SC-build.cpp
Linking SC-build
Starting SC-build
SC-build "configure" started
librarySource    = "/Users/user/Developer/Projects/SC"
toolSource       = "/Users/user/Developer/Projects/SC/Tools"
outputs          = "/Users/user/Developer/Projects/SC/_Build"
projects         = "/Users/user/Developer/Projects/SC/_Build/_Projects"
SC-build "configure" finished (took 72 ms)
 ~/Developer/Projects/SC > 
```

Build all projects
```
./SC.sh build compile
```

Possible Output:
```
~/Developer/Projects/SC > ./SC.sh build compile     
Starting SC-build
SC-build "compile" started
librarySource    = "/Users/user/Developer/Projects/SC"
toolSource       = "/Users/user/Developer/Projects/SC/Tools"
outputs          = "/Users/user/Developer/Projects/SC/_Build"
projects         = "/Users/user/Developer/Projects/SC/_Build/_Projects"
/Applications/Xcode.app/Contents/Developer/usr/bin/make clean
Cleaning SCTest
Creating /Users/user/Developer/Projects/SC/_Build/_Projects/Make/../../_Intermediates/SCTest/macOS-arm64-make-clang-Debug
Creating /Users/user/Developer/Projects/SC/_Build/_Projects/Make/../../_Outputs/macOS-arm64-make-clang-Debug
Compiling Async.cpp
Compiling AsyncTest.cpp
Compiling BuildTest.cpp
... other files..
Compiling DebugVisualizersTest.cpp
Compiling SC-format.cpp
Compiling ToolsTest.cpp
Compiling SC-package.cpp
Generate compile_commands.json
Linking SCTest
SC-build "compile" finished (took 3071 ms)
```

# SC-package.cpp

`SC-package` downloads third party tools needed for Sane C++ development (example: `clang-format`).  
Proper OS / Architecture combination is selected (Windows/Linux/Mac and Intel/ARM) and all downloaded files (_packages_) `MD5` hash is checked for correctness.  
_Packages_ are placed and extracted in `_Build/_Packages` and once extracted, they are symlinked in `_Build/Tools`.  

@note Directory naming has been chosen to avoid clashes when mounting the same working copy folder in multiple concurrent virtual machines with different Operating Systems / architectures.  
This happens during regular development, where new code is frequently tested in parallel on macOS, Windows and Linux before even committing it and pushing it to the CI system.

## Actions

- `install`: Downloads requires tools (LLVM / 7zip)

## Examples

```
./SC.sh package install
```

## Packages
These are the packages that are currently downloaded and extracted / symlinked by `SC-package.cpp`:

- `LLVM 15`: Downloads LLVM from the official github repository
- `7zip`: 7zip executable (needed to decompress LLVM installer on Windows)
- `7zr.exe`: 7Zip console executable (needed to decompress 7zip installer on Windows)
- `doxygen`: Doxygen documentation generator
- `doxygen-awesome-css`: Doxygen awesome css (Doxygen theme)

# SC-format.cpp

`SC-format` tool formats or checks formatting for all files in Sane C++ repository

## Actions
- `execute`: Formats all source files in the repository in-place
- `check`: Does a dry-run format of all source files. Returns error on unformatted files (used by the CI)

## Examples

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
- Two `.cpp` files are compiled (only if they're out of date) by a `Makefile`
    - `Tools/Tools.cpp` file 
        - Includes the `SC.cpp` unity build file, defining `main(argc, argv)`
    - `$TOOL.cpp` file
        - Implements the specific tool functionality
- The first argument after `$TOOL` name is the main `$ACTION` by convention
- `$ACTION` and all `$PARAM_X` arguments passed to the bootstrap shell scripts are forwarded to the tool
- The resulting executable is run with the forwarded arguments

Example:  
`SC.sh $TOOL $ACTION $PARAM_1 $PARAM_2 ... $PARAM_N`

@note `Tools.cpp` is shared between all compiled tools by the `Makefile` inside the `_Build/_Tools/_Intermediates` directory.  
This allows modifying and recompiling a script, or compiling different scripts spending negligible (often sub-second) time, to just compile/link script logic.  
This is because all Sane C++ Libraries are compiled just once (the first time, in no more than a couple of seconds) inside `Tools.cpp`.

# Roadmap

- Generate ready made .vscode configurations to debug the programs easily
- When and if [SC::Build](@ref library_build) will be capable of launching build commands autonomously, get rid of the tool `Tools\Build\$(PLATFORM)` makefiles.
- Investigate better way of expressing the dependencies chain between scripts
- Investigate if Tools (scripts) can be sandboxed using os facilities
- Do not require to have a C++ toolchain already installed on the system [*]

[*] Running the Sane C++ Tools scripts on a machine without a pre-installed compiler could be implemented by shipping a pre-compiled `SC-package.cpp` (that is already capable of downloading `clang` on all platforms) or by adding to the bootstrap script ability to download a suitable compiler (and sysroot).

