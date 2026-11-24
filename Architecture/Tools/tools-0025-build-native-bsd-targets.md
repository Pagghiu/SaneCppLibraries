# TOOLS-0025 - Build native BSD targets with distinct identities

Status: Accepted
Date: 2026-11-02

The exported-symbol restriction is superseded by
[TOOLS-0026](tools-0026-export-symbols-from-native-bsd-hosts.md).

## Context

The native build backend needs to distinguish NetBSD, FreeBSD, and OpenBSD from Linux: their base compilers, digest
libraries, runtime facilities, sanitizer behavior, and object tools differ. A foreign target selected on a BSD host can
otherwise reach unqualified toolchain discovery or package installation. NetBSD 11's tested GCC sanitizer cannot run
with the guest's default ASLR policy. FreeBSD's Clang C++ driver selects a C++ sanitizer archive even for strict projects
that intentionally omit the C++ runtime.

## Decision

Represent each BSD as a separate build platform, default to a host-native target, and use its ELF linker, symbol, and
strip operations for the tested native workflow. Preserve each platform in output paths and library link settings.
When the native backend resolves a target on a BSD host, reject a foreign operating system or architecture before
selecting a toolchain. Project-file generation for supported generators remains available.

Leave NetBSD Native Debug ASan disabled unless the project explicitly requests it. On native FreeBSD Clang links with
sanitizers enabled and `linkStdCpp` disabled, use the C driver when the configured linker is the default C++ driver. This
links the requested ASan and UBSan instrumentation without the C++ ABI archive. Keep FreeBSD Debug sanitizers opt-in
until concurrent fork and thread startup are qualified. Do not change system or executable ASLR settings.

## Consequences

FreeBSD library integration needs base `libmd`; NetBSD and OpenBSD use libc for hashing. A native BSD backend only
selects qualified host toolchains; cross-building BSD ARM64 from Linux or macOS is a separate decision in TOOLS-0028.

## Confirmation

Compile and run native Debug and Release targets, including a digest known-answer test, on all three guests. Check
platform identity, required system libraries, native foreign-target rejection, and project generation. On FreeBSD,
explicitly link and run a strict target with ASan and UBSan, verify its dependencies, and confirm intentional heap bounds
and signed-overflow violations produce diagnostics. Retain existing-platform suites and generated single-file builds.

## OpenBSD extension

OpenBSD uses its own platform identity, the native ELF build path, and the base Clang toolchain. Link pthread
and libexecinfo. The runtime loader and the BSD digest routines come from libc. The native Debug preset leaves ASan
disabled because the OpenBSD 8.0 base compiler set lacks its runtime.

The qualification covers native OpenBSD 8.0 amd64 builds and the shared suite. The bootstrap uses the shared POSIX tools
implementation, including the kqueue backends for Async and FileSystemWatcher. FileSystem and Testing use OpenBSD 8.0's
`getexecpath` for executable-path discovery. HttpClient uses the shared pthread and libcurl backend.

The built-in workspace uses Git to resolve pinned GUI dependencies during configuration. Cross-build profiles remain
separately qualified.
