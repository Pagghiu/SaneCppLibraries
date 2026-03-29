# Hot Reload And Export Recipes

Use this reference when a user wants a working recipe for plugin hosts, hot reload, or host symbol exports.

## Recipe 1: Host Built With SC::Build

- Configure the host executable in `SC-build.cpp`.
- Call `SC::Build::Project::addExportLibraries(...)` with the smallest useful set.
- Use `addExportAllLibraries()` only when broad export is acceptable.
- On Linux, let `SC::Build` add the needed `-rdynamic` behavior.

## Recipe 2: Host Built Without SC::Build

- Define `SC_EXPORT_LIBRARY_<LIBRARY>=1` on the host executable target.
- Define the macro set only for the libraries that the plugin will actually use.
- Add `-rdynamic` on Linux when runtime symbol resolution is required.

## Recipe 3: Hot Reload Loop

- Read plugin metadata with `PluginScanner`.
- Register loaded plugins in `PluginRegistry`.
- Watch the plugin folder with `FileSystemWatcher`.
- Reload dependent plugins when a `.cpp` changes.

## Best Reference Paths

- `Documentation/Libraries/Plugin.md`
- `Documentation/Pages/Build.md`
- `Tests/Libraries/Plugin/PluginTest.cpp`
- `Examples/SCExample/HotReloadSystem.h`
- `Tools/SC-build.cpp`

## Pitfalls

- Do not put export defines on the plugin build.
- Do not forget Linux symbol export flags.
- Do not treat hot reload as safe without explicit ownership and shutdown rules.
