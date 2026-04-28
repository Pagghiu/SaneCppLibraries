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
#include "Tools/SC-build.h"

SC::Result SC::Build::configure(Definition &definition, const Parameters &parameters)
{
    Project project = {"MyProject"};
    SC_TRY(addSaneCppLibraries(project, parameters));
    SC_TRY(project.addFiles("Source", "main.cpp"));
    return definition.addProject(move(project)); // Added to implicitly created Workspace
}

```

Example `Source/main.cpp` using Sane C++ Libraries:

```cpp
#include "Libraries/Strings/Console.h"

int main()
{
    using namespace SC;
    Console console;
    console.print("{1} {0}\n", "a tutti", "Salve");
    return 0;
}
```

`parameters.directories.projectDirectory` is the project root discovered by the launcher.  
`parameters.directories.libraryDirectory` is the SaneCppLibraries checkout that provides the generic build driver and
the public headers.

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

# `SC_BUILD` Define

When `SC-build.cpp` itself is compiled as the build-definition tool, `SC::Build` defines `SC_BUILD=1`.

This lets one file act as both the build definition and the target source:

```cpp
#if defined(SC_BUILD)
#include "Tools/SC-build.h"

SC::Result SC::Build::configure(Definition &definition, const Parameters &parameters)
{
    Project project = {"MyProject"};
    SC_TRY(project.setRootDirectory(parameters.directories.projectDirectory.view()));
    SC_TRY(project.addPresetConfiguration(Configuration::Preset::Debug, parameters));
    SC_TRY(project.addPresetConfiguration(Configuration::Preset::Release, parameters));
    SC_TRY(project.addFile("SC-build.cpp"));
    return definition.addProject(move(project));
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
ThirdParty/SaneCppLibraries/SC-build.sh compile MyProject --config Release --generator native
```

Explicit shared checkout:

```bash
/path/to/SaneCppLibraries/SC-build.sh --libraries-root /path/to/SaneCppLibraries compile MyProject --config Release --generator native
```

Standalone downloaded launcher:

```bash
curl -L -o SC-build.sh https://raw.githubusercontent.com/Pagghiu/SaneCppLibraries/<branch-or-tag>/SC-build.sh
chmod +x SC-build.sh
./SC-build.sh compile MyProject --config Release --generator native
```

Windows PowerShell:

```powershell
Invoke-WebRequest https://raw.githubusercontent.com/Pagghiu/SaneCppLibraries/<branch-or-tag>/SC-build.ps1 -OutFile SC-build.ps1
./SC-build.ps1 compile MyProject --config Release --generator native
```

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
mode is less reproducible.

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
