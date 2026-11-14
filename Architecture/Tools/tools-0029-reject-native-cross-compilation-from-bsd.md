# TOOLS-0029 - Reject native cross-compilation from BSD

Status: Accepted
Date: 2026-11-13

## Context

TOOLS-0025 establishes a host-native build path for NetBSD and FreeBSD. A foreign target selected on either
BSD host could reach toolchain discovery or package installation, although those cross-build combinations
have no qualified toolchain and sysroot. Project-file generation itself already works on both BSD hosts.

## Decision

When the native backend resolves the target, use the actual build machine to reject a foreign operating
system on NetBSD and FreeBSD before selecting a toolchain. Retain the existing checks on native BSD targets
and allow BSD hosts to generate project files for platforms with supported generators.

## Consequences

The native backend requires the target operating system and architecture to match the BSD build machine.
Configuring a foreign generated project remains available. Compiling those projects requires a host and
toolchain that support their target.

## Confirmation

Exercise native cross-target rejection through the build action on BSD and other hosts. Verify on NetBSD
and FreeBSD that normal project configuration still generates the other platforms' projects and native
SCTest compiles and runs in Debug and Release.
