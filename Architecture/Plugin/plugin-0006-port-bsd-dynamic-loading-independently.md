# PLUGIN-0006 - Port BSD dynamic loading and native compilation

Status: Accepted
Date: 2026-11-03

## Context

NetBSD, FreeBSD, and OpenBSD provide the POSIX dynamic-loader API used by Plugin. Their base C++ toolchains differ, and
compiler discovery, native plugin builds, and watcher-driven reloads must not be inferred from the loader alone.

## Decision

Use Plugin's existing POSIX dynamic-library implementation on all three BSD systems. Select `g++` as a GNU compiler on
NetBSD and `clang++` as a Clang compiler on FreeBSD and OpenBSD, for both compilation and linking. Resolve these names
through the process search path when invoked; selection does not guarantee that the executable is installed. Callers
may supply their own compiler configuration. If a platform has no qualified automatic selection,
`PluginCompiler::findBestCompiler` returns `CompilerNotFound` rather than success with empty compiler paths.

## Consequences

The existing Plugin compiler and registry can build and explicitly load native plugins on the tested BSD systems.
Support remains experimental and does not qualify every BSD toolchain, ABI, plugin dependency graph, or deployment.

## Confirmation

Exercise shared loader error behavior on supported platforms and load, resolve, call, close, and reload a real shared
library on all three BSD systems. Build and reload a fixture through the Plugin registry, including a directory path
containing spaces, and verify that unsupported compiler discovery reports a failure with its structured detail.

## Related

- [PLUGIN-0003 - Make Plugin runtime and sysroot policy explicit](plugin-0003-make-plugin-runtime-and-sysroot-policy-explicit.md)
- [Plugin documentation](../../Documentation/Libraries/Plugin.md)
