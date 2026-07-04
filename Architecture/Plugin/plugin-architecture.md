# Plugin Architecture

## Purpose

Plugin provides source-defined, runtime-compiled, in-process C++ plugin loading for hot reload and host/plugin contracts. It must make the compile/link/load/reload workflow explicit enough that toolchain, sysroot, runtime, metadata, and crash-risk decisions stay visible.

## Architectural Shape

The public interface is organized around definitions, scanning, compilation, dynamic libraries, sysroots, compiler environment, and registry storage. Plugin definitions come from source comments. The scanner parses bounded metadata. The compiler builds source files into native dynamic libraries. The registry uses caller-provided storage to compile, load, unload, reload, and query plugins.

Metadata and registry structures must stay bounded or caller-provided. Internal filesystem and string helpers belong to Plugin when they exist only to keep Plugin independent from higher-level Sane libraries.

## Boundaries

Plugin owns the in-process hot-reload path for source-delivered plugins. It may depend on Process for invoking compilers and File through that dependency chain, but it should avoid broad dependencies for metadata, scanning, filesystem traversal, time, strings, containers, or allocation policy.

Plugin does not provide a security sandbox, hostile-code isolation, stable binary plugin ABI, or general build-system replacement.

## Similarities With Other Libraries

Plugin follows the project-wide explicit-storage and explicit-runtime-policy pattern. Like Process, it handles external tools through visible `Result` failures. Like Time and Socket, it has had dependency pressure reduced by pushing convenience behavior into local helpers and primitive types.

## Differences From Other Libraries

Unlike ordinary platform wrappers, Plugin intentionally loads native code into the current process. Unlike Process, it extends the host executable instead of isolating execution in a child process. Unlike Build, it compiles one plugin-oriented shape directly instead of modeling a full project graph.

## Inspirations

The evidenced inspiration is hot reload of C++ source into native dynamic libraries, including the SCExample usage documented by Plugin. Native dynamic loader and compiler/sysroot concepts are part of the architecture because Plugin intentionally works at that platform boundary.

## Anti-Inspirations

Explicit anti-inspirations include closed-source prebuilt binary plugin distribution and unconfigured different-host/different-plugin compiler models, both named as non-goals or unplanned work in the documentation. Inferred anti-inspirations include scripting runtimes that hide compilation and plugin managers that allocate unbounded metadata internally.

## Architectural Choices

- Keep plugins source-defined and compiled on demand.
- Keep dynamic loading in-process unless a future ADR adds another execution mode.
- Keep metadata and registry storage bounded or caller-provided.
- Keep host exports, sysroot selection, libc/libc++ use, and runtime stubs explicit.
- Keep plugin dependency reload behavior visible through `PluginRegistry`.

## Explicitly Excluded Targets

- Sandboxing or crash isolation for untrusted plugins.
- Shipping a stable binary plugin ecosystem.
- Becoming a full build system.
- Requiring STL containers or Sane owning containers for metadata.
- Hiding host symbol export or sysroot policy.

## Sources

- [Plugin documentation](../../Documentation/Libraries/Plugin.md)
- [Plugin public API](../../Libraries/Plugin/Plugin.h)
- [Plugin macros](../../Libraries/Plugin/PluginMacros.h)
- [Plugin internal string helpers](../../Libraries/Plugin/Internal/PluginString.h)
- [Plugin tests](../../Tests/Libraries/Plugin/PluginTest.cpp)
- [PLUGIN-0001 - Compile source-defined plugins on demand and load them in-process](plugin-0001-compile-source-defined-plugins-on-demand-and-load-them-in-process.md)
- [PLUGIN-0002 - Keep Plugin metadata and registry bounded and dependency-light](plugin-0002-keep-plugin-metadata-and-registry-bounded-and-dependency-light.md)
- [PLUGIN-0003 - Make Plugin runtime and sysroot policy explicit](plugin-0003-make-plugin-runtime-and-sysroot-policy-explicit.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0016 - Support layered adoption modes](../Global/sc-0016-support-layered-adoption-modes.md)

## Decision Log

- [PLUGIN-0001 - Compile source-defined plugins on demand and load them in-process](plugin-0001-compile-source-defined-plugins-on-demand-and-load-them-in-process.md)
- [PLUGIN-0002 - Keep Plugin metadata and registry bounded and dependency-light](plugin-0002-keep-plugin-metadata-and-registry-bounded-and-dependency-light.md)
- [PLUGIN-0003 - Make Plugin runtime and sysroot policy explicit](plugin-0003-make-plugin-runtime-and-sysroot-policy-explicit.md)
