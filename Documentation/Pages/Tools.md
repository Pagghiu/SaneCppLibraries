@page page_tools Tools

Sane C++ tools are small C++ programs compiled on demand by the repository bootstrap. They automate repository work
without introducing another scripting runtime, and they exercise the same libraries that application code uses.

[TOC]

# Why tools are C++ programs

A tool can use `Process`, `FileSystem`, `Hashing`, `Http`, and the other libraries directly. It can be debugged with a
normal C++ debugger, share result-handling conventions with the repository, and run on macOS, Linux, and Windows.

The tradeoff is explicit: a working host C++ compiler is required to bootstrap the first tool. After that, unchanged
tool support code is reused from `_Build`, so ordinary invocations only rebuild what changed.

# Invoke a built-in tool

The first argument to `SC.sh` or `SC.bat` selects a tool. The next argument selects an action owned by that tool:

```text
./SC.sh <tool> <action> [tool arguments...]
```

For example:

```bash
./SC.sh build compile SCTest Debug
./SC.sh package status llvm
./SC.sh format check
```

The words after the tool name matter. `build compile` stops after producing the target; `build run` brings the target up
to date and then launches it. Arguments after `--` are forwarded to the launched program.

External projects use the dedicated `SC-build.sh`, `SC-build.ps1`, or `SC-build.bat` launchers described in
[SC::Build (External use)](@ref page_build_external).

# Write a small custom tool

Pass the path of a tool source file instead of a built-in name:

```bash
./SC.sh MyTools/InspectFiles.cpp inspect Source
```

A tool implements `Tools::Tool::runTool` and receives the selected action, remaining arguments, repository paths, and a
console:

```cpp
#include "Libraries/Strings/Console.h"
#include "Tools/Tools.h"

namespace SC
{
namespace Tools
{
StringView Tool::getToolName() { return "InspectFiles"; }
StringView Tool::getDefaultAction() { return "inspect"; }

Result Tool::runTool(Tool::Arguments& arguments)
{
    arguments.console.print("Action: {0}\n", arguments.action);
    for (StringView argument : arguments.arguments)
        arguments.console.printLine(argument);
    return Result(true);
}
} // namespace Tools
} // namespace SC
```

Use this form for repository-local automation that benefits from Sane C++ APIs. A standalone application with its own
targets belongs in [SC::Build](@ref page_build) instead.

# SC-build.cpp

`SC-build` reads the repository's C++ build definition and performs `compile`, `run`, `coverage`, `documentation`, or
generated-project `configure` actions. Learn its model and first workflow in [SC::Build](@ref page_build).

Keep two rules in mind:

- compile explicitly before test runs when you want build and test failures reported as separate steps;
- put executable arguments after `--`, so they are not parsed as build options.

# SC-package.cpp

`SC::Package` acquires development tools, compilers, sysroots, and runners used by repository workflows. It is not an
application dependency manager: packages live under `_Build` and support building, formatting, documentation, and
cross-target validation.

## Think in recipes, receipts, and exports

Every package has three useful layers:

1. A recipe describes how the package is downloaded, imported, prepared, and validated.
2. A receipt records what was installed and which recipe version produced it.
3. Exports name the tools, sysroots, runners, include directories, or libraries that another tool may consume.

After installation or import, validation produces a receipt and a set of named exports. This allows `SC::Build` to ask
for a capability such as an LLVM compiler or QEMU runner without reconstructing an installation path from directory
naming conventions.

## Inspect before changing package state

Start with read-only actions:

```bash
./SC.sh package list
./SC.sh package info llvm
./SC.sh package status llvm
./SC.sh package exports llvm
```

`status` reports whether a receipt is present and structurally valid without failing merely because a package is
missing. `verify` performs the stricter validation used when a workflow depends on that package:

```bash
./SC.sh package verify llvm
```

When validation fails, `doctor` explains the observed state and suggests a next action:

```bash
./SC.sh package doctor llvm
```

## Install, import, or repair

Install a package managed by the built-in catalog:

```bash
./SC.sh package install llvm
```

Some large or host-specific tools can be registered from an existing directory instead of downloaded:

```bash
./SC.sh package install qemu --import-directory /opt/qemu-user
```

Fil-C 0.685 is available on native Linux Intel64 (`x86_64`) and ARM64 (`aarch64`) hosts. `package install filc`
selects the host-matching archive; an imported compiler must report the matching Linux target triple. The resulting
toolchain capability is `toolchain.filc.x86_64` or `toolchain.filc.aarch64`, respectively.

Download the host-matching release with:

```bash
./SC.sh package install filc
```

To register an existing ARM64 installation, use:

```bash
./SC.sh package install filc --import-directory /home/user/filc-0.685-linux-aarch64
```

For an Intel64 host, use the corresponding `filc-0.685-linux-x86_64` import directory.

Fil-C runs also prepare package-local `zlib-filc`, `openssl-filc`, `nghttp2-filc` and `curl-filc` libraries and prepend their library directories to
`LD_LIBRARY_PATH`. Host libraries are not ABI-compatible with Fil-C. No system library is replaced.

```bash
./SC.sh package install curl-filc
./SC.sh package verify curl-filc
./SC.sh package install openssl-filc
./SC.sh package verify openssl-filc
./SC.sh package install nghttp2-filc
./SC.sh package verify nghttp2-filc
```

`curl-filc` builds pinned curl 8.22.0 sources with the native Fil-C compiler using upstream configure and make.
The profile supports HTTP/HTTPS, HTTP/2, proxies and threaded DNS. It builds compatible OpenSSL 3.6.5 and nghttp2
1.70.0 dependencies; WebSockets and automatic content decoding remain disabled. HttpClient reports actual loaded-library
features. OpenSSL also supplies Cryptography's optional runtime backend, without a link dependency in consumer libraries.
Building these packages requires host `make`, `patch`, Perl and the usual configure prerequisites. OpenSSL uses the
project-maintained [Fil-C port rebase](https://github.com/Pagghiu/SaneCppLibraries/blob/filc/Tools/Support/FilCOpenSSLPort.md), retaining its assembly bridge.
curl defaults to the Linux CA bundle `/etc/ssl/certs/ca-certificates.crt`; requests can supply a custom CA file.
Cache identity includes source, compiler, port patch and relevant dependency identities. No system libraries are replaced.

The experimental Fil-C CI builds SCTest in Release and runs it through `Support/Scripts/RunFilCTLSFixture.sh Release`
on every push and pull request, manual dispatch, and weekly on the default branch. Both native x86_64 and ARM64
are checked. Fil-C CI omits SaneHttpGet and single-file library compilation; those retain normal compiler coverage.
The Posix matrix also tests Ubuntu 26.04 explicitly.
This native Linux fixture requires OpenSSL capabilities and exercises HttpClient's trusted custom CA, required HTTP/2
response, wrong CA and hostname rejection. Ordinary runs without fixture variables report an explicit transfer-test
skip. Its public test-only credentials must never be used for a real service or installed into a system trust store.
Functional ABI and protocol checks do not replace upstream OpenSSL tests or establish side-channel resistance.

`repair` is for a recognized existing layout whose current receipt or launcher metadata can be reconstructed. It is not
a generic substitute for a failed install:

```bash
./SC.sh package repair llvm-mingw
```

## Record the resolved environment

`lock` writes `_Build/SC-package.lock`, a local summary of installed receipts and exports:

```bash
./SC.sh package lock
```

The lock file makes a development environment auditable. It does not turn imported packages into content-addressed
artifacts and should not be mistaken for an application dependency lock.

Use `./SC.sh package help` for the current package catalog and action syntax. The registry and recipe API live in
`Tools/SC-package` when an external tool needs to compose its own package entries.

# SC-format.cpp

`SC::Format` applies the repository's pinned clang-format configuration to supported source files.

## Format a working tree

```bash
./SC.sh format execute
```

Run this after editing C or C++ sources, then inspect the diff. Formatting does not decide whether generated files,
comments, or declaration grouping are sensible.

## Check without modifying files

```bash
./SC.sh format check
```

`check` exits with an error when formatting would change a file, which makes it suitable for CI and pre-commit
validation. Both actions use the package-managed formatter version expected by the repository.

# How does it work

The bootstrap keeps startup work incremental. It rebuilds `ToolsBootstrap` only when stale, locates the selected tool
source, reuses unchanged `Tools.cpp` support objects, compiles the tool when needed, and then runs it with the original
action and arguments.

`ToolsBootstrap` is deliberately small. The selected tool is linked with the Sane C++ unity build, so custom tools can
use the libraries without maintaining another project definition. Build products remain below `_Build/_Tools`.

# Know the boundary

Use a tool for a bounded development operation with a command-line lifecycle. Use SC::Build for a graph of application
targets. Use a library when behavior belongs in reusable program code. Keeping those roles separate prevents the
bootstrap from becoming an application framework.
