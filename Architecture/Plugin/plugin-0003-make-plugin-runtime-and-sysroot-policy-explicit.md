# PLUGIN-0003 - Make Plugin Runtime and Sysroot Policy Explicit

Status: Accepted
Date: 2026-07-04

## Context

Runtime-compiled C++ plugins sit at an awkward boundary between the host executable, the platform dynamic loader, compiler toolchains, sysroots, libc/libc++ availability, exported Sane symbols, and minimal runtime shims. Making those choices implicit would produce fragile and hard-to-debug plugin builds.

## Decision

Plugin runtime and sysroot policy is explicit. Hosts opt into exporting the Sane libraries plugins need. `PluginCompiler`, `PluginSysroot`, and `PluginCompilerEnvironment` describe compiler paths, include/library paths, build options, and environment-derived flags. `PluginMacros` define plugin entry points and the optional runtime/linker definitions used by plugin dynamic libraries.

## Consequences

Plugin users must configure host exports and toolchain/sysroot choices deliberately, especially outside `SC::Build`. The library can support no-runtime, libc, and libc++-aware plugin builds without making one policy mandatory for every host. The public API carries toolchain concepts because they are part of the plugin architecture.

## Confirmation

A change preserves this decision when host-export requirements remain documented, compiler/sysroot/environment choices are represented by explicit Plugin types or build options, Plugin tests cover relevant runtime configurations, and new runtime stubs or allocation hooks are not hidden in unrelated libraries.

## Related

- [Plugin documentation: exporting SC symbols](../../Documentation/Libraries/Plugin.md)
- [Plugin compiler and sysroot API](../../Libraries/Plugin/Plugin.h)
- [Plugin macros](../../Libraries/Plugin/PluginMacros.h)
- [Plugin tests](../../Tests/Libraries/Plugin/PluginTest.cpp)
- [FOUNDATION-0003 - Keep C++ runtime shims optional and Foundation-owned](../Foundation/foundation-0003-keep-cpp-runtime-shims-optional-and-foundation-owned.md)
- [SC-0016 - Support layered adoption modes](../Global/sc-0016-support-layered-adoption-modes.md)
