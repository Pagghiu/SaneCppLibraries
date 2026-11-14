# PLUGIN-0007 - Select native BSD plugin compilers

Status: Accepted
Date: 2026-11-03

## Context

The Plugin dynamic loader is available on the tested NetBSD and FreeBSD guests. Their base C++ toolchains differ:
the tested NetBSD system uses GCC, while the tested FreeBSD system uses Clang. Plugin compilation and linking use
compiler-specific options, so selecting the matching type matters.

## Decision

Select `g++` as a GNU compiler on NetBSD and `clang++` as a Clang compiler on FreeBSD, for both compilation and
linking. As with the existing Unix defaults, these names are resolved through the process search path when invoked;
selection does not guarantee that the executable is installed. Callers may supply their own compiler configuration.

## Consequences

The existing Plugin compiler and registry can build and explicitly load native plugins on the tested BSD systems.
This remains experimental and does not establish watcher-driven reload support or qualify every BSD toolchain, ABI,
or plugin dependency configuration.

## Confirmation

Run a shared compile-and-load smoke test on supported platforms. On NetBSD and FreeBSD also build and reload a fixture
through the actual Plugin registry, including a directory path containing spaces.

## Related

- [PLUGIN-0006 - Port BSD dynamic loading independently](plugin-0006-port-bsd-dynamic-loading-independently.md)
- [Plugin documentation](../../Documentation/Libraries/Plugin.md)
