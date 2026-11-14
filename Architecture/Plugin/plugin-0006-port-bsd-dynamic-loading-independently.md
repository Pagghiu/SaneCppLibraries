# PLUGIN-0006 - Port BSD dynamic loading independently

Status: Accepted
Date: 2026-11-03

## Context

NetBSD and FreeBSD provide the POSIX dynamic-loader API used by Plugin. Compiler discovery, native plugin builds, and
watcher-driven reloads require separate platform work and must not be inferred from the loader alone.

## Decision

Use Plugin's existing POSIX dynamic-library implementation on NetBSD and FreeBSD. Until automatic compiler selection
is implemented and qualified on a platform, `PluginCompiler::findBestCompiler` returns `CompilerNotFound` rather than
success with empty compiler paths.

## Consequences

Applications can use the experimental low-level loader with an explicit library path on the tested BSDs. This does
not claim full Plugin compilation or hot-reload support, and callers can still supply their own compiler configuration.

## Confirmation

Exercise shared loader error behavior on supported platforms and load, resolve, call, close, and reload a real shared
library on NetBSD and FreeBSD. Verify that unsupported compiler discovery reports a failure with its structured detail.

## Related

- [PLUGIN-0003 - Make Plugin runtime and sysroot policy explicit](plugin-0003-make-plugin-runtime-and-sysroot-policy-explicit.md)
- [Plugin documentation](../../Documentation/Libraries/Plugin.md)
