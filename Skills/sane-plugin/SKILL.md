---
name: sane-plugin
description: Hot-reload plugin guidance for third-party AI agents using Sane C++ Libraries. Use when building runtime-loaded plugins, configuring host exports, scanning plugin sources, or debugging reload and symbol-visibility issues.
---

# Sane Plugin

## Quick Start

- Use this skill when you need to compile `.cpp` plugins on the fly, reload them, or define host/plugin contracts.
- Start with [plugin-lifecycle-and-host-exports.md](references/plugin-lifecycle-and-host-exports.md).

## Core Workflow

1. Inspect `Tests/Libraries/Plugin/PluginTest.cpp` for the canonical flow.
2. Use `PluginDefinition` and `PluginScanner` to read plugin metadata.
3. Use `PluginCompiler`, `PluginSysroot`, and `PluginRegistry` to compile, load, reload, and unload.
4. Route larger hot-reload systems to `Examples/SCExample/HotReloadSystem.h`.

## Host Export Rules

- Export exactly the Sane libraries the plugin needs.
- Prefer `SC::Build::Project::addExportLibraries(...)` or `addExportAllLibraries()` when the host uses `SC::Build`.
- If the host is not built with `SC::Build`, define `SC_EXPORT_LIBRARY_<LIBRARY>=1` on the host executable build.
- On Linux, also export the host symbols with `-rdynamic`.

## Common Pitfalls

- Do not recommend binary plugins by default.
- Do not put export defines on the plugin build instead of the host build.
- Keep shutdown and reload ownership explicit so reloading does not leak state.

## References

- [plugin-lifecycle-and-host-exports.md](references/plugin-lifecycle-and-host-exports.md)
- `Tests/Libraries/Plugin/PluginTest.cpp`
- `Examples/SCExample/HotReloadSystem.h`
- `Documentation/Libraries/Plugin.md`
