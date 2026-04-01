# Plugin Lifecycle And Host Exports

Use this reference when a user needs runtime-loaded plugin support, hot reload, or host symbol visibility.

## What This Library Does

- Compile plugin `.cpp` files into dynamic libraries at runtime.
- Load, unload, and reload plugins while keeping dependency order visible.
- Let plugins query host or peer interfaces through `PluginDynamicLibrary::queryInterface`.

## Recommended Workflow

1. Start from `Tests/Libraries/Plugin/PluginTest.cpp` to learn the API shape.
2. Use `PluginDefinition::find` and `PluginDefinition::parse` to read plugin metadata.
3. Use `PluginScanner::scanDirectory` to build the plugin catalog.
4. Use `PluginCompiler` and `PluginSysroot` to configure compilation.
5. Use `PluginRegistry` to load, reload, and unload plugin graphs.

## Host Export Checklist

- Export the exact Sane libraries the plugin needs.
- If the host uses `SC::Build`, prefer `SC::Build::Project::addExportLibraries(...)`.
- Use `SC::Build::Project::addExportAllLibraries()` only when broad export is acceptable.
- If the host is not built with `SC::Build`, define the matching `SC_EXPORT_LIBRARY_<LIBRARY>=1` macros on the host executable target.
- On Linux, ensure the host exports its symbols with `-rdynamic`.

## Common Companion Paths

- `Documentation/Libraries/Plugin.md`
- `Tests/Libraries/Plugin/PluginTest.cpp`
- `Examples/SCExample/HotReloadSystem.h`

## Pitfalls

- Treat plugins as source-delivered `.cpp` files, not prebuilt binaries.
- Keep shutdown and reload ownership explicit.
- Do not mix up plugin compilation flags with host export flags.
