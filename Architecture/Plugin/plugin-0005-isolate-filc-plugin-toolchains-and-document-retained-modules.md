# PLUGIN-0005 - Isolate Fil-C Plugin Toolchains and Document Retained Modules

Status: Accepted
Date: 2026-10-06

## Context

Fil-C plugins can compile and call the existing SC interfaces, but host-compiled plugins are ABI
incompatible. Runtime instrumented library search paths also cause native compiler tools such as
the system linker to load instrumented zlib and crash. Successful dlclose is not physical unloading.

## Decision

Keep the existing ClangCompiler path. Fil-C discovery requires SC_FILC_PLUGIN_COMPILER and optionally
SC_FILC_PLUGIN_LINKER, defaulting the latter to the compiler. SC-Build supplies its resolved drivers
when running a Fil-C executable. Missing configuration fails rather than selecting host g++.

Clear LD_LIBRARY_PATH only in Fil-C compiler/linker subprocesses, leaving the host and native compiler
behavior unchanged. Custom native compiler dependencies belong in an explicitly configured launcher.

Enable compile/load/interface coverage on both Fil-C distributions. Exclude only same-path reload
assertions on Fil-C glibc, where reload requests return ReloadUnsupported before changing live state.
Document that close ends the plugin instance and logical handle lifetime,
not module residency; unload destructors and bounded-memory repeated reload are unsupported.

## Consequences

External hosts must configure compatible toolchains. Rootless reload remains experimental and can retain
every module version. Explicit plugin close remains responsible for cleanup. This does not add physical
unloading support to Fil-C or make cross-distribution/cross-version plugins safe.

## Confirmation

PluginTest covers configured discovery and an unconfigured child, both default plugin compilation and
C++-header-only compilation, dependency loading, interfaces and logical close. Rootless additionally
checks modified code on reload. Native toolchain environments and tests remain unchanged.

## Related

- [PLUGIN-0003](plugin-0003-make-plugin-runtime-and-sysroot-policy-explicit.md)
- [Plugin documentation](../../Documentation/Libraries/Plugin.md)
