# Sane Plugin Build Recipes

## Quick Start

- Use this guide when the user needs Plugin plus Build to work together cleanly.
- Start with [hot-reload-and-export-recipes.md](references/hot-reload-and-export-recipes.md).

## Recipe Order

1. Decide whether the host uses `SC::Build` or an external build system.
2. Configure host exports for only the Sane libraries the plugin needs.
3. On Linux, add `-rdynamic` to the host executable when plugin symbols must resolve at runtime.
4. Add hot-reload support using `Plugin`, `FileSystemWatcher`, and optionally `Async`.

## What To Emphasize

- Plugins are source-delivered `.cpp` files.
- The export defines belong on the host executable, not on the plugin.
- `SC::Build::Project::addExportLibraries(...)` is the minimal-path helper.
- `SC::Build::Project::addExportAllLibraries()` is the broad-path helper.
- `Examples/SCExample/HotReloadSystem.h` is the best end-to-end reference.

## Common Pitfalls

- Do not forget that Linux host executables need symbol export flags.
- Do not suggest binary plugins as the default path.
- Do not mix plugin reload state with build-generation state.

## References

- [hot-reload-and-export-recipes.md](references/hot-reload-and-export-recipes.md)
- `Documentation/Libraries/Plugin.md`
- `Documentation/Pages/Build.md`
- `Tests/Libraries/Plugin/PluginTest.cpp`
- `Examples/SCExample/HotReloadSystem.h`
