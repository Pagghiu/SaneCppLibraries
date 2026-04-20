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
  - `linux-glibc-x86_64`
  - `linux-glibc-arm64`
  - `linux-musl-x86_64`
  - `linux-musl-arm64`
  - `windows-gnu-x86_64`
  - `windows-gnu-arm64`
  - `windows-msvc-x86_64`
  - `windows-msvc-arm64`
- `build --toolchain` is now part of the public surface too. Current values include `default`, `host-default`, `clang`, `filc`, `gcc`, `msvc`, `clang-cl`, and `llvm-mingw`.
- `build run` can use `--runner` and `--runner-path` for foreign executables.
- Linux target profiles now shape canonical target triples and sysroot flags, and macOS / Windows hosts auto-select a packaged LLVM toolchain for them when explicit compiler paths are absent. On macOS, packaged Linux glibc and musl sysroots are now part of that first-class path too, so `linux-glibc-*` and `linux-musl-*` can compile without an explicit `--sysroot`.
- Foreign Linux targets can now use the native runner model through `qemu-user`; `build run --runner qemu` or `--runner auto` passes `-L <sysroot>` so dynamic Linux binaries resolve against the configured sysroot.
- On macOS and Linux, supported Windows targets can route through Wine. On Linux x64 console targets, the runner still prefers `wineconsole --backend=curses` when that sibling executable exists, while Linux arm64 stays on plain `wine`.
- Linux arm64 now also has a real targeted `windows-gnu-arm64` smoke path through the packaged native ARM64 Wine runner.
- Portable MSVC acquisition is part of the native-backend story now: `SC-package install msvc` can acquire or import the hosted `cl/link/lib` toolchain plus Windows SDK, and accepts `--import-directory` / `--wine` overrides for imported layouts.
- `SC-package install llvm` is now the packaged host-toolchain entry point for Linux-target native-backend work on non-Linux hosts.
- `SC-package install filc` is the experimental compiler-first entry point for Linux Fil-C work. Pair it with `build ... --toolchain filc`, and keep the explanation scoped to native Linux `x86_64` output rather than a public target-profile row.
- `SCBuildTest` fixture runs now reuse the repository `_Build/_PackagesCache` and `_Build/_Packages` roots for large packaged toolchains and runners, while still isolating each run's projects, outputs, intermediates, and build cache under `_Build/_Tests/...`.
- Once portable MSVC is installed, later native `windows-msvc-*` builds can reuse the wrapper path recorded in `sc-msvc-package.json` instead of depending on `SC_MSVC_WINE` again.
- Existing portable MSVC layouts can now repair missing metadata and wrapper scripts in place, and SDK version detection falls back from `Windows Kits/10/bin` to `Include` or `Lib` when SDK tools are absent.
- Existing packaged Linux Wine runners now also repair their launcher scripts in place, so portable MSVC wrapper updates do not require deleting the cached runner package first.
- Portable MSVC caches are now host-specific, so shared macOS/Linux workspaces do not reuse the wrong recorded Wine wrapper path across hosts.
- On Linux arm64, portable MSVC installation can auto-generate `box64 + wine64` wrappers for the hosted x64 MSVC tools, while native Wine execution now selects a separate ARM64 Wine runner for `windows-*-arm64` targets.
- On Linux x64, console targets can still prefer `wineconsole --backend=curses`; on Linux arm64, the native runner stays on plain `wine`.
- When Linux arm64 lacks a usable system `box64`, the packaged Wine runner can now resolve a maintained generic-arm `box64` package automatically; together with the packaged native ARM64 Wine runner, that path now validates clean native-backend `SCTest` compiles for both `windows-msvc-x86_64` and `windows-msvc-arm64`, plus targeted `BaseTest/new-delete` runs for both targets.

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
- Do not advertise Fil-C as fully supported yet; it is still an experimental compiler track until the first clean Linux compile/start path and CI coverage land.
- Keep host export rules on the executable target.
