# FOUNDATION-0004 - Delegate Fil-C Glibc TLS Cleanup To Bundled ABI

Status: Accepted
Date: 2026-10-04

## Context

Fil-C 0.685 glibc does not export the `__cxa_thread_atexit_impl` symbol used by Foundation's ordinary glibc shim.
Its bundled libc++abi implements TLS destructor registration and the necessary thread-exit fallback.

## Decision

For Fil-C/glibc only, omit Foundation's optional C++ ABI shim definitions and link the bundled libc++abi through
the explicit `filc-glibc` build profile. Do not duplicate ABI definitions or substitute process-exit cleanup for thread-exit destruction.
Keep existing native and pizfix shim behavior unchanged. C++ standard-library headers and libc++ itself remain
excluded in strict SC targets; this profile uses the compiler's ABI support, not STL containers or exceptions.

## Consequences

The experimental glibc profile requires a preinstalled `/opt/fil` distribution including its ABI library.
Rootless Fil-C dependencies and build outputs must not be mixed with this profile. Compiler upgrades require
runtime validation; the profile does not modify host libraries or automatically perform privileged installation.

## Confirmation

Build strict SCTest with `--toolchain filc-glibc` and verify thread-local destruction at thread join. Native and
rootless compiler profiles must retain their previous link flags and runtime-shim behavior.
