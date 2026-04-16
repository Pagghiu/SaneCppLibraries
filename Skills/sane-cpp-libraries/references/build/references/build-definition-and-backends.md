# Build Definition And Backends

Use this reference when a user needs to configure `SC::Build`, understand backends, or export Sane libraries for plugins.

## What This Library Does

- Describe builds imperatively in `SC-build.cpp`.
- Generate Xcode, Visual Studio, or Make projects.
- Run a native backend directly on macOS and Linux, including supported cross-target workflows.

## Recommended Workflow

1. Read `Documentation/Pages/Build.md` for the overall model.
2. Inspect `Tools/SC-build.cpp` for real repository configuration.
3. Inspect `Tests/Libraries/Build/BuildTest.cpp` for generator and backend behavior.
4. Use `SC::Build::Definition`, `Workspace`, `Project`, and `Configuration` to model the build.
5. Select the backend that matches the user’s workflow.

## Native Backend Notes

- Select `SC::Build::Generator::Native` when the user wants direct builds without generated IDE files.
- The native backend is available on macOS and Linux.
- It launches compiler, linker, archiver, and supported runner processes itself.
- It now resolves build, host, target, and runner roles up front.
- Friendly target profiles should be the default explanation surface:
  - `windows-gnu-x86_64`
  - `windows-gnu-arm64`
  - `windows-msvc-x86_64`
  - `windows-msvc-arm64`
- `build run` can use `--runner` and `--runner-path` for foreign executables.
- On macOS and Linux, supported Windows targets can route through Wine. On Linux console targets, the runner prefers `wineconsole --backend=curses` when that sibling executable exists.
- Portable MSVC acquisition is part of the native-backend story now: `SC-package install msvc` can acquire or import the hosted `cl/link/lib` toolchain plus Windows SDK, and accepts `--import-directory` / `--wine` overrides for imported layouts.
- Once portable MSVC is installed, later native `windows-msvc-*` builds can reuse the wrapper path recorded in `sc-msvc-package.json` instead of depending on `SC_MSVC_WINE` again.
- Existing portable MSVC layouts can now repair missing metadata and wrapper scripts in place, and SDK version detection falls back from `Windows Kits/10/bin` to `Include` or `Lib` when SDK tools are absent.
- Existing packaged Linux Wine runners now also repair their launcher scripts in place, so portable MSVC wrapper updates do not require deleting the cached runner package first.
- Portable MSVC caches are now host-specific, so shared macOS/Linux workspaces do not reuse the wrong recorded Wine wrapper path across hosts.
- On Linux arm64, both portable MSVC installation and native Wine execution can auto-generate `box64 + wine64` wrappers when those host tools are present.
- When Linux arm64 lacks a usable system `box64`, the packaged Wine runner can now resolve a maintained generic-arm `box64` package automatically; that path now validates a real native-backend `windows-msvc-x86_64` `SCTest` compile, while run support and broader ARM64-target validation are still being finished.

## Plugin Export Notes

- Use `SC::Build::Project::addExportLibraries(...)` for the minimal export set.
- Use `SC::Build::Project::addExportAllLibraries()` only when a broad host export is acceptable.
- On Linux, exported-plugin hosts also need `-rdynamic`.

## Common Companion Paths

- `Tools/SC-build.cpp`
- `Tests/Libraries/Build/BuildTest.cpp`
- `Documentation/Pages/Build.md`

## Pitfalls

- Do not oversell `SC::Build` as the only way to consume the libraries.
- Do not confuse generated-project backends with the native backend.
- Do not flatten build support and run support into one statement; the support matrix is intentionally narrower for some runner and architecture pairs.
- Keep host export rules on the executable target.
