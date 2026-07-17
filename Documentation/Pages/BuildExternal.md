@page page_build_external SC::Build (External use)

Use `SC::Build` in another repository when you want a small C++ build description, direct native builds, and optional
access to the Sane C++ libraries without copying this repository's project structure.

[TOC]

# What the external launcher does

An external project owns its `SC-build.cpp`. The launcher finds a compatible SaneCppLibraries checkout, compiles the
build description, and executes the requested action from the external project directory.

```text
MyProject/SC-build.cpp
        |
        v
SC-build launcher ----> resolve a SaneCppLibraries revision
        |
        v
compile the definition ----> build MyProject
```

The external project remains in control of its sources and revision pin. SaneCppLibraries supplies the build API,
bootstrap, and any libraries selected by the definition.

# Create the smallest project

Start with this layout:

```text
MyProject/
  SC-build.cpp
  Source/
    main.cpp
```

`SC-build.cpp`:

```cpp
// sc-build-version: main
#include "SaneCppBuild.h"

SC::Result SC::Build::configure(Definition& definition, const Parameters& parameters)
{
    Project project = {"MyProject"};
    project.setRootDirectory(parameters.directories.projectDirectory.view());
    SC_TRY(project.addFiles("Source", "*.cpp"));
    SC_TRY(addSaneCppLibraries(project, parameters));
    return definition.addProject(move(project));
}
```

`Source/main.cpp`:

```cpp
#include "SaneCppStrings.h"

int main()
{
    SC::Console console;
    console.print("Hello from {0}!\n", "Sane C++");
    return 0;
}
```

`addSaneCppLibraries` adds the public include directory and the unity-build library source. Pass `Libraries::Multiple`
as its third argument only when you specifically want separate library compilation.

# Choose where the libraries live

There are three useful layouts. Choose one deliberately rather than letting local machine state decide.

## Vendored checkout

Keep SaneCppLibraries below the project, for example `ThirdParty/SaneCppLibraries`. This is easy to inspect and works
without network access after checkout, but updating the dependency changes the project tree.

```bash
ThirdParty/SaneCppLibraries/SC-build.sh compile MyProject Debug
```

## Shared checkout

Point the launcher at an existing checkout while developing both projects together:

```bash
/path/to/SaneCppLibraries/SC-build.sh --libraries-root /path/to/SaneCppLibraries compile MyProject Debug
```

This is convenient for local development. It is not a reproducible dependency declaration by itself.

## Standalone launcher and cache

Download the small launcher into the project and let it maintain one shared clone plus versioned worktrees in the user
cache. This keeps the dependency outside the project while allowing the revision pragma to select an exact checkout.

```bash
curl -L -o SC-build.sh https://raw.githubusercontent.com/Pagghiu/SaneCppLibraries/main/SC-build.sh
chmod +x SC-build.sh
./SC-build.sh compile MyProject Debug
```

On Windows, download `SC-build.ps1`; `SC-build.bat` is a wrapper for command-prompt use.

# Pin the revision

The cache flow reads a pragma near the top of `SC-build.cpp`:

```cpp
// sc-build-version: 5ab42c5abfea35e9c148e18ff244563b593979a2
```

A branch is useful while collaborating on active development. A tag or commit SHA is a better choice when another
person or CI must reproduce the same build. Without the pragma, the launcher follows the current default branch and
warns that the result is not pinned.

# Keep paths project-relative

External definitions may be launched from nested directories and may use a cached library checkout. Resolve project
files from `parameters.directories.projectDirectory`; do not derive them from the location of SaneCppLibraries.

# Troubleshooting by layer

- `Cannot find SC-build.cpp`: run inside the project tree or pass `--project-dir`.
- Revision resolution failure: check `sc-build-version`, network access, or use `--libraries-root` with a known checkout.
- Bootstrap compiler failure: verify that a working host C++ compiler is available.
- Source files not found: set the project root and keep source patterns relative to it.
- Wrong library revision: remove an accidental shared-checkout override and inspect the revision printed by the launcher.

Once the first target builds, continue with the [SC::Build guide](@ref page_build) to add configurations, generated
projects, or cross targets.
