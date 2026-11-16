# TOOLS-0025 - Build native BSD targets with distinct identities

Status: Accepted
Date: 2026-11-02

The exported-symbol restriction is superseded by
[TOOLS-0026](tools-0026-export-symbols-from-native-bsd-hosts.md).
The FreeBSD strict Debug sanitizer policy is superseded by
[TOOLS-0027](tools-0027-link-strict-freebsd-sanitizers-with-the-c-driver.md).

## Context

The native build backend needs to distinguish NetBSD and FreeBSD from Linux: their base compilers, digest libraries and
runtime facilities differ. The initial goal is a small native target, not an assertion that every build feature or library
is available on either BSD. NetBSD 11's tested GCC sanitizer cannot run with the guest's default ASLR policy.

## Decision

Represent NetBSD and FreeBSD as separate build platforms, default to a host-native target, and use their ELF linker and
strip operations for the tested native workflow. Preserve each platform in output paths and library link settings.
Initially reject BSD cross-targets, alternate generated backends, target triples, sysroots and exported-symbol selection
instead of applying another platform's defaults. The NetBSD Native Debug preset leaves ASan disabled unless the project
explicitly requests it. FreeBSD retains the existing default when the project links the C++ runtime; without it, the
tested sanitizer requires missing C++ ABI symbols, so the preset does not enable ASan automatically. Do not change system
or executable ASLR settings.

## Consequences

Small custom targets can build before the full `SC.cpp` or SCTest port is ready. FreeBSD library integration needs base
`libmd`; NetBSD uses libc for hashing and does not link `libdl`. Native build success does not qualify unported
FileSystemWatcher, package installers, cross builds, exported symbols or other untested features.

## Confirmation

Compile and run native Debug and Release targets, including a digest known-answer test, on both guests. Check output
platform identity, default and explicit sanitizer policy, required system link libraries and failure of unsupported
options. Retain existing-platform Debug/Release suites and generated single-file compilation.

## OpenBSD extension

OpenBSD uses its own platform identity, the native ELF build path, and the base Clang toolchain. Link pthread
and libexecinfo. The runtime loader and the BSD digest routines come from libc. The native Debug preset leaves ASan
disabled because the OpenBSD 8.0 base compiler set lacks its runtime.

The initial qualification covers native OpenBSD 8.0 amd64 builds and the filesystem iteration, process, threading,
socket, and hashing libraries. The bootstrap uses the shared POSIX tools implementation, including the existing kqueue
backends for Async and the file watcher. The smoke harness uses the absolute invocation path supplied by the build
tool for tests that need the executable or its directory. Automatic executable path lookup remains open.

The built-in workspace needs Git for GUI dependency resolution before compiling SCTest. With that configuration
step omitted in a diagnostic checkout, SCTest reaches the HttpClient compile, where OpenBSD still needs native
condition-variable support. Cross-build profiles and incremental fiber stacks also require separate qualification.
