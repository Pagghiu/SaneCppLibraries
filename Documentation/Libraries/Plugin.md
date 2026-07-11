@page library_plugin Plugin

@brief 🟨 Compile source-delivered C++ extensions and reload them inside a running process

[TOC]

`Plugin` is for applications that want a short edit–compile–reload loop without introducing a separate build-system invocation for every extension. It discovers C++ source plugins, invokes an installed compiler and linker, loads the resulting dynamic library into the host process, and coordinates reloads across declared plugin dependencies.

That makes it a plausible fit for developer-facing desktop tools, live UI iteration, and applications whose extensions can deliberately share the host's C++ ABI. It is not a general-purpose package format or a security boundary. Plugins run native code in the host process, and a bad pointer, an ABI mismatch, or incomplete shutdown can crash the application.

[SaneCppPlugin.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppPlugin.h) is the amalgamated distribution.

# Dependencies
- Dependencies: [Process](@ref library_process)
- All dependencies: [File](@ref library_file), [Process](@ref library_process)

![Dependency Graph](Plugin.svg)


# The model: source tree to live instance

A plugin is a directory containing one or more `.cpp` files. One file carries a comment block between `SC_BEGIN_PLUGIN` and `SC_END_PLUGIN`; the scanner derives the plugin identifier from the directory name and parses human-readable metadata, build options, and other plugin identifiers it depends on.

The host then drives four distinct stages:

1. `PluginScanner` walks the source tree and writes `PluginDefinition` objects into caller-provided storage.
2. `PluginCompiler` and `PluginSysroot` describe the compiler, include paths, system headers, and libraries. Compilation produces object files beside the plugin sources; linking produces a platform dynamic library.
3. `PluginRegistry` owns neither an unbounded plugin list nor a scheduler. The host supplies a `Span<PluginDynamicLibrary>`, installs scanned definitions, and explicitly asks to load or reload an identifier.
4. `PluginDynamicLibrary` holds the native library handle, plugin instance, reload count, and last compiler/linker error. Optional typed contracts are obtained with `queryInterface`.

Loading a plugin recursively loads its declared dependencies. Unloading does the reverse: the registry first unloads plugins that depend on the requested one. This ordering is necessary because all instances and interfaces are ordinary in-process C++ objects, not isolated handles.

# A representative host loop

The following SCTest-backed example shows the complete shape: discover definitions, configure the toolchain, provide bounded registry storage, watch the source directory, and reload affected plugins. The production [SCExample](@ref page_examples) expands the same idea with error display and state serialization across reloads.

\snippet Tests/Libraries/Plugin/PluginTest.cpp PluginSnippet

The example is intentionally explicit about policy. `Plugin` does not create a watcher, debounce file events, preserve application state, or decide what to do with a compiler error. [FileSystemWatcher](@ref library_file_system_watcher) can report changes, while the host remains responsible for scheduling reloads and presenting `PluginDynamicLibrary::lastErrorLog`.

# The plugin-side contract

`SC_PLUGIN_DEFINE(MyPlugin)` exports the conventional `MyPluginInit` and `MyPluginClose` entry points. Initialization allocates one plugin instance and calls its `init()` method; shutdown calls `close()` and deletes it. `SC_PLUGIN_EXPORT_INTERFACES` optionally exports a query function that maps compile-time `PluginHash` values to base-class interfaces.

The interface is a C++ ABI contract despite the C entry points used to find it. Host and plugin must agree on compiler ABI, data layout, compile definitions, and the exact interface header. `queryInterface` returns a pointer into the plugin instance; it becomes invalid before or during unload. Do not cache that pointer across a reload.

The test plugins under `Tests/Libraries/Plugin/PluginTestDirectory` are the smallest concrete examples: the child declares a dependency on the parent, implements two interface bases, and exports them with `SC_PLUGIN_EXPORT_INTERFACES`. The host test verifies that reloading replaces executable behavior and that unloading the parent also unloads the dependent child.

# Allocation and lifetime boundaries

The library follows SC's caller-storage model for discovery and registration:

- `PluginScanner::scanDirectory` receives the definition array and a growable temporary file buffer. It fails rather than silently exceeding the supplied definition capacity.
- Each definition has fixed limits: 8 dependencies, 8 build options, and 10 source files; identifiers and metadata also use fixed strings. These limits are part of the current API, not merely implementation details.
- `PluginRegistry::init` receives storage for every registered `PluginDynamicLibrary`. `replaceDefinitions` fails when that capacity is insufficient.
- Compilation and linking are subprocess operations. Their captured diagnostics are copied into the dynamic library's fixed 8 KiB error storage and exposed through `lastErrorLog`, so long diagnostics can be truncated.

Plugin instances are the important exception to the library-wide no-allocation expectation: `SC_PLUGIN_DEFINE` constructs the plugin with `new` and destroys it with `delete`. In the default runtime-free build, those operators are supplied by the macro and route to `SC::Memory`. Any state owned by the plugin must be released by `close()` or its destructor before the library is unloaded. The registry must itself be closed while the host services used by plugin shutdown are still alive.

Interfaces and objects must not straddle the unload boundary. A robust reload sequence stops asynchronous work, releases callbacks and interface pointers, optionally serializes host-owned state, reloads, queries fresh interfaces, and then restores state. The SCExample `HotReloadSystem` demonstrates that orchestration; the Plugin library does not attempt to make arbitrary live C++ state reload-safe.

# Linking against the host

By default, plugins are compiled without the C or C++ runtime and resolve functionality from the loading executable or from dependency plugins. This keeps the extension surface centered on SC libraries, but it means the host must export every SC library used by plugin code.

With `SC::Build`, opt in with `SC::Build::Project::addExportLibraries` or `addExportAllLibraries`. Regular executables do not export SC symbols automatically. For example, a host that exposes only Foundation, Memory, and Strings passes those three library names to `addExportLibraries`.

Without `SC::Build`, define the corresponding switches consistently for the host target, for example `SC_EXPORT_LIBRARY_FOUNDATION=1`, `SC_EXPORT_LIBRARY_MEMORY=1`, and `SC_EXPORT_LIBRARY_STRINGS=1`. On Linux the executable must additionally expose its symbols to the dynamic loader, normally with `-rdynamic`. The [Build documentation](@ref page_build) covers the equivalent project configuration.

A plugin metadata block can request `libc` and `libc++` through its `Build` field. `PluginSysroot` then supplies the system include and library paths. That is useful when an extension genuinely needs the standard runtime, but it increases the ABI surface and does not make mismatched host/plugin toolchains safe. `findBestCompiler` and `findBestSysroot` are conveniences for local developer machines, not deployment guarantees; production tooling should be prepared to configure paths explicitly and report discovery failures.

# Relationships and tradeoffs

`Plugin` deliberately delegates adjacent concerns:

- [Process](@ref library_process) runs the compiler and linker. Plugin adds the command construction and platform dynamic-library policy.
- [FileSystemWatcher](@ref library_file_system_watcher) detects edits. Plugin only maps a changed relative path to affected plugin identifiers, with a caller-selected time tolerance.
- [Build](@ref page_build) configures which host symbols are exported. Plugin is not a replacement for building the host application.
- `SystemDynamicLibrary` is the low-level loading mechanism. Plugin adds source discovery, compilation, lifecycle entry points, dependency ordering, and interface lookup.

Compared with a conventional binary plugin SDK, source delivery gives the host control over compilation and makes hot reload straightforward, but requires a compatible toolchain on the machine and exposes plugin source. Compared with an out-of-process or WASM extension, native in-process plugins have cheap direct calls and can reuse host types, but provide no fault containment, stable wire protocol, or sandbox.

# Platform status and practical limits

🟨 **MVP.** The implementation targets macOS, Windows, and Linux with Clang, GCC, and MSVC-style toolchains. It includes platform-specific host linking and Windows handling for debugger-held DLL/PDB files. The repository exercises real compile, link, interface-query, reload, dependency-unload, and runtime-free standard-header cases.

The current design is most credible as a development-time hot-reload facility under control of the application team. Shipping it as an end-user extension system requires additional decisions around compiler distribution, source trust, ABI compatibility, diagnostics, versioning, and recovery from plugin faults. Precompiled closed-source plugins, cross-compiler ABI support, sandboxing, RPC isolation, and WASM hosting are not implemented features.

# Further reading

- [SCExample](@ref page_examples) contains the fuller `HotReloadSystem` built from `Plugin` and `FileSystemWatcher`.
- [Building as a library user](@ref page_building_user) describes manual host export switches.
- [Ep.22 – Hot-Reload dear imgui](https://www.youtube.com/watch?v=BXybEWvSpGU)
- [Ep.24 – Hot-Reload C++ on iOS](https://www.youtube.com/watch?v=6DfykfYCQdY)
- [Ep.25 – C++ Serialization and Reflection (with Hot-Reload)](https://www.youtube.com/watch?v=d7DXxC6xG_A)
- [June 2024 update](https://pagghiu.github.io/site/blog/2024-06-30-SaneCppLibrariesUpdate.html) and [July 2024 update](https://pagghiu.github.io/site/blog/2024-07-31-SaneCppLibrariesUpdate.html) provide historical context.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Plugin`.
Single File counts
`SaneCppPlugin.h`.
Standalone counts `SaneCppPluginStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 865		| 1322		| 2187	|
| Single File | 1688		| 1977		| 3665	|
| Standalone  | 4275		| 5617		| 9892	|
