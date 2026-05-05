@page page_build_external External SC::Build Bootstrap

`SC::Build` can be used outside of the Sane C++ repository through the `SC-build.sh`, `SC-build.bat`, and
`SC-build.ps1` launchers.

[TOC]

# Overview

External projects provide a regular `SC-build.cpp` build-definition file and let the launcher resolve a SaneCppLibraries
checkout to compile and run it.

The launcher supports three layouts:

- Vendored checkout inside the project, for example `ThirdParty/SaneCppLibraries`
- Shared checkout outside the project, selected explicitly with `--libraries-root <path>`
- Standalone launcher script using a shared cache clone plus versioned worktrees

The project entry file is always named `SC-build.cpp`.

# Project Layout

Minimal external project:

```text
MyProject/
  SC-build.cpp
  Source/
    main.cpp
```

Example `SC-build.cpp`:

```cpp
#include "SaneCppBuild.h"

SC::Result SC::Build::configure(Definition &definition, const Parameters &parameters)
{
    Project project = {"MyProject"};
    SC_TRY(Build::addSaneCppLibraries(project, parameters)); // Link Sane C++ libraries single file
    SC_TRY(project.addFiles("Source", "*.cpp"));             // Add all cpp files from Source folder
    return definition.addProject(move(project));             // Added to implicitly created workspace
}
```

Example `Source/main.cpp` using Sane C++ Libraries:

```cpp
#include "SaneCppStrings.h"

int main()
{
    using namespace SC;
    Console console;
    console.print("{1} {0}!\n", "world", "Hello");
    return 0;
}
```

__Note__: the public `Includes` folder is added by `Build::addSaneCppLibraries` so that `main.cpp` can `#include "SaneCppStrings.h"`.


# Using Sane C++ Libraries

For casual external projects, `SC::Build` can wire Sane C++ Libraries into a target with one line:

```cpp
SC_TRY(addSaneCppLibraries(project, parameters));
```

This default uses `SC.cpp`.

If you prefer non-unity compilation under `SC::Build`, use:

```cpp
SC_TRY(addSaneCppLibraries(project, parameters, Libraries::Multiple));
```

`Libraries::SingleFile` is the default and is recommended for the simplest onboarding.  
`Libraries::Multiple` adds the individual files under `Libraries/`.

This function adds the public `Includes` folder to include paths, to allow `#include "SaneCpp$LIBRARY$.h"`.

# `SC_BUILD` Define

When `SC-build.cpp` itself is compiled as the build-definition tool, `SC::Build` defines `SC_BUILD=1`.

This lets one file act as both the build definition and the target source:

```cpp
#if defined(SC_BUILD)
#include "SaneCppBuild.h"

SC::Result SC::Build::configure(Definition &definition, const Parameters &parameters)
{
    Project project = {"MyProject"};
    SC_TRY(Build::addSaneCppLibraries(project, parameters)); // Link Sane C++ libraries
    SC_TRY(project.addFile("SC-build.cpp"));                 // Flag this file to build
    return definition.addProject(move(project));             // Add to default workspace
}
#else
#include <stdio.h>

int main()
{
    puts("hello from the built program");
    return 0;
}
#endif
```

# Launchers

Vendored checkout:

```text
MyProject/
  SC-build.cpp
  ThirdParty/
    SaneCppLibraries/
      SC-build.sh
```

Invoke from the project root or any nested subdirectory:

```bash
ThirdParty/SaneCppLibraries/SC-build.sh
ThirdParty/SaneCppLibraries/SC-build.sh compile MyProject --config Release
```

With no action, the launcher defaults to `compile` and uses the native generator.

Explicit shared checkout:

```bash
/path/to/SaneCppLibraries/SC-build.sh --libraries-root /path/to/SaneCppLibraries
/path/to/SaneCppLibraries/SC-build.sh --libraries-root /path/to/SaneCppLibraries compile MyProject --config Release
```

Standalone downloaded launcher:

```bash
curl -L -o SC-build.sh https://raw.githubusercontent.com/Pagghiu/SaneCppLibraries/<branch-or-tag>/SC-build.sh
chmod +x SC-build.sh
./SC-build.sh
./SC-build.sh compile MyProject --config Release
```

Windows:

```powershell
Invoke-WebRequest https://raw.githubusercontent.com/Pagghiu/SaneCppLibraries/<branch-or-tag>/SC-build.ps1 -OutFile SC-build.ps1
./SC-build.ps1
./SC-build.ps1 compile MyProject --config Release
```

The `SC-build.bat` wrapper forwards to `SC-build.ps1`, so either launcher can be used from `cmd.exe` or PowerShell.

When testing a non-`main` branch, download the launcher from that branch and pin the same branch in `SC-build.cpp`:

```cpp
// sc-build-version: build
```

# Version Pinning

When the launcher uses the shared cache flow, it reads the requested SaneCppLibraries revision from a comment pragma in
`SC-build.cpp`:

```cpp
// sc-build-version: v0.1.0
```

The value can be a tag, branch, or commit SHA.

If the pragma is missing, the launcher resolves the latest default-branch revision and prints a warning because that
mode is less reproducible. Add the pragma near the top of `SC-build.cpp` once you know which SaneCppLibraries revision
the external project should follow:

```cpp
// sc-build-version: 5ab42c5abfea35e9c148e18ff244563b593979a2
#include "SaneCppBuild.h"
```

Branch pins are convenient while developing against an active branch. Tags or commit SHAs are better for projects that
need repeatable builds.

# Shared Cache

If `--libraries-root` and `SC_BUILD_LIBRARIES_ROOT` are both absent, the standalone launcher uses a shared cache:

- POSIX: `$XDG_CACHE_HOME/sc-build` or `~/.cache/sc-build`
- Windows: `%LOCALAPPDATA%\SC-build`

The cache stores:

- One source clone of `SaneCppLibraries`
- One detached worktree per resolved commit SHA

Overrides:

- `--libraries-root <path>`: use an explicit checkout and disable cache resolution
- `--project-dir <path>`: choose the project root explicitly instead of searching upward from the current directory
- `SC_BUILD_LIBRARIES_ROOT`: environment variable equivalent of `--libraries-root`
- `SC_BUILD_CACHE_DIR`: override the shared cache base directory

# Troubleshooting

- `Cannot find SC-build.cpp`: run the launcher from inside the project tree or pass `--project-dir <path>`
- `Cannot resolve SaneCppLibraries revision`: fix the `// sc-build-version: ...` pragma or use `--libraries-root`
- `git is required`: install `git` or use `--libraries-root` pointing at an existing checkout
- `ToolsBootstrap` build failures: make sure a working host compiler is installed and available
- Wrong relative paths inside the build definition: use `parameters.directories.projectDirectory` as the project root
