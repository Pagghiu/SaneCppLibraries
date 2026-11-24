# TOOLS-0028 - Cross-build BSD ARM64 with release sysroots

Status: Accepted

## Context

Compiling Release SCTest on emulated ARM64 guests took 12–14 minutes, while executing it took
about one minute. The runtime suite needs the actual BSD kernel and userland.

## Decision

Allow Linux and macOS hosts to build the FreeBSD and NetBSD ARM64 targets using the Native
backend, Clang, LLD and an explicit sysroot. Use LLVM object tools for the target ELF files.
Keep native BSD toolchain selection and GNU linker flags for native NetBSD builds. This extends the host-side
cross-target restriction in TOOLS-0025 without allowing unqualified cross compilation from BSD hosts.

CI pins matching release archives by digest and builds on Ubuntu x86_64. FreeBSD and NetBSD
use separate jobs. Each job boots its guest once and runs the staged Debug and Release
executables from their normal checkout-relative paths. Guest-native plugin fixture compilation
continues to exercise the plugin compiler and linker.

NetBSD installs its base-system C++ headers under `/usr/include/g++`, which an upstream Clang
cross compiler does not discover from `--sysroot` alone. When a NetBSD cross-built target opts
into standard C++ headers, add that target-owned directory as a system include path. Native
NetBSD keeps using its compiler's normal header discovery, and strict no-standard-C++ targets
keep the existing `-nostdinc++` boundary.

## Confirmation

Exercise the shared SCTest suite in both configurations on FreeBSD and NetBSD ARM64.
Compile the generated single-file targets in both configurations, including the Await targets
that require the system C++ headers. Report host compile and guest run times separately.

## OpenBSD extension

The `openbsd-arm64` profile uses the OpenBSD 8.0 base and compiler sets for the sysroot. Cross-builds
use LLVM's name-only symbol output. Native OpenBSD keeps the base `nm` POSIX format. The ARM64 job
compiles on Ubuntu and runs the existing suite inside the OpenBSD guest.

The OpenBSD release sets contain versioned shared libraries. Create unversioned linker names inside
the cross sysroot so upstream LLD can select the shared libraries and record their versioned SONAMEs.
Check for a shared libc dependency before staging the test executable.
