@page page_build SC::Build

`SC::Build` describes a C++ build in C++ and can either execute it directly or generate projects for another build
system. This guide explains that model and the shortest useful workflow.

[TOC]

# The build model

An `SC-build.cpp` file is an executable build description. The repository launcher compiles it when needed, asks it to
create a graph of workspaces, projects, configurations, and files, then hands that graph to a backend.

The pipeline has three stages: the launcher compiles a stale `SC-build.cpp`, the definition creates the project graph,
and the selected backend consumes that graph. The native backend compiles and links immediately; generated backends
write Xcode, Visual Studio, or Make projects for another tool to build.

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

## Experimental BSD tier

NetBSD 11, FreeBSD 15.1, and OpenBSD 8.0 form a lower support tier than macOS, Windows, and Linux. Their verified scope
is release-specific and does not imply support for other BSD systems or releases. GitHub Actions runs the shared suite
on x86_64 guests and runs cross-built tests in ARM64 guests for all three systems.

Native builds have been verified with `SCTest` in Debug and Release on NetBSD with GCC and FreeBSD with Clang. Run the
standard configure, compile, and run commands from a guest-local checkout.
On either the NetBSD or FreeBSD host, `build configure` generates the usual Visual Studio, Xcode, and Make projects for Windows,
macOS, and Linux. It also prepares the local native build directories. BSD targets require the Native backend
for native and cross-builds. From Linux or macOS with Clang, LLD and LLVM binutils on PATH, cross-build an
ARM64 test executable with a release-matched sysroot:

```sh
./SC.sh build compile SCTest Release --target freebsd-arm64 --sysroot /path/to/freebsd-sysroot
./SC.sh build compile SCTest Release --target netbsd-arm64 --sysroot /path/to/netbsd-sysroot
./SC.sh build compile SCTest Release --target openbsd-arm64 --sysroot /path/to/openbsd-sysroot
```

Run the executable in a matching BSD guest. See the validation script below for the checkout layout.

Run `sh Tests/SCTest/BSD/run.sh` from the guest checkout to validate both native configurations and the
generated single-file libraries. The README in `Tests/SCTest/BSD` lists the runtime prerequisites and
how to use headers generated in a matching checkout.

Native Debug leaves ASan off by default on both BSDs. The tested NetBSD GCC sanitizer was incompatible with the
guest's ASLR policy, and the FreeBSD Clang sanitizer could deadlock during concurrent fork and thread startup.
Projects may enable it explicitly when their toolchain and workload support it. Sane C++ library integration supplies
the base system link libraries, including `libmd` on FreeBSD.

### Native OpenBSD

Experimental OpenBSD support uses OpenBSD 8.0 and base Clang. Install Git for pinned build
dependencies and curl for the HttpClient tests. From a guest-local checkout, run the shared suite in both configurations.
The tested base compiler set does not provide the AddressSanitizer runtime, so native Debug builds remain unsanitized.

```sh
for configuration in Debug Release; do
    ./SC.sh build compile SCTest "$configuration" &&
        ./SC.sh build run SCTest "$configuration" -- --all-tests || exit 1
done
```

The OpenBSD port uses kqueue for Async and file watching, with buffered Async file sends. The real
certificate-validation and HTTP/2 loopback fixture runs on OpenBSD amd64 and ARM64 with HTTP/2-enabled libcurl.
The optional Cryptography provider can use the OpenBSD OpenSSL 3.5 package (`pkg_add 'openssl%3.5'`). The base
LibreSSL library supplies the platform WebSocket digest path. Incremental fiber stacks use the native fault context
on amd64 and ARM64.

The BSD GitHub Actions workflow runs the shared `SCTest --all-tests` suite in Debug and Release on amd64 and ARM64.
It also builds the generated single-file targets on amd64. The ARM64 job cross-builds on Ubuntu x86_64 using
LLVM 22 and the release sysroot, then runs the executables inside the OpenBSD guest. Both jobs require the
OpenSSL 3 provider and incremental stack growth, then reuse the Debug and Release executables for the real HTTP/2
fixture and synthetic libcurl runtime matrix. See `Tests/SCTest/BSD/README.md` for the shared run commands.

### BSD support matrix

| Area | Verified experimental BSD coverage | Requirements and limits |
| --- | --- | --- |
| Native build | Shared SCTest suite on NetBSD 11, FreeBSD 15.1, and OpenBSD 8.0; all 56 generated single-file targets and executable exports for plugins | Generated BSD targets and cross-builds from BSD hosts are unsupported. |
| Filesystem and Async | File copying and iteration, recursive watches with NetBSD/OpenBSD kqueue and FreeBSD inotify in threaded and event loop modes, timers, sockets, and process I/O | Watchers use bounded inventory storage. NetBSD and OpenBSD Async file sends use buffered I/O. FreeBSD uses native `sendfile`. |
| Memory and fibers | Protected reservations, incremental fiber stack growth, and AsyncFibers | Incremental stack growth on BSD is available on ARM64 and x86_64. The runtime-owned stack mode requires Fil-C on glibc. |
| Hashing and cryptography | MD5, SHA-1, SHA-256, native entropy, and OpenSSL 3 provider tests on all three BSDs | Native BSD cryptography supplies entropy. AES and HMAC require the OpenSSL 3 provider. FreeBSD hashing links `libmd`; OpenBSD uses the packaged OpenSSL 3.5 runtime. |
| Compression and HTTP | zlib streams, HTTP and WebSocket suites, and a separate TLS and HTTP/2 loopback fixture | HttpClient loads libcurl at runtime. TLS and HTTP/2 depend on its capabilities. |
| Plugins | Native compilation, diamond dependencies, interface calls, and reload from a watcher event | The host exports the libraries used by the plugin. The host and plugin need a compatible C++ ABI and an acyclic dependency graph. |
| Packages | Pinned Git assets and SHA-256-pinned archive installation; base `ftp`/`fetch` fallback on NetBSD and FreeBSD | Built-in binary toolchain and cross-build installers report an unsupported host. |
| Serial ports | POSIX pseudoterminal tests | Physical devices remain unverified. |

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

# Experimental Fil-C toolchain

On Linux, `--toolchain filc` selects the native host architecture. The rootless Fil-C 0.687 packages are available for Intel64
(`x86_64`) and ARM64 (`aarch64`); the imported compiler's target triple must match the host. Fil-C remains toolchain-only
and does not add a `linux-filc-*` target profile.

Install the archive for the current host, then build without an explicit cross-target:

```bash
./SC.sh package install filc
./SC.sh build compile SaneHttpGet Debug --toolchain filc
```

The ARM64 distribution is experimental and requires a 4 KiB kernel page size. Archive setup requires `patchelf`.
Use `--import-directory` only for an installation that has already completed upstream's `setup.sh`.

# Continue from here

- Use [SC::Build (External use)](@ref page_build_external) to adopt the build system outside this repository.
- Use [Building (Contributor)](@ref page_building_contributor) for the repository validation loop.
- Read `Tools/SC-build/Build.h` when you need the exact public API.
- Read `Tools/SC-build.cpp` for the build graph used by this repository.
