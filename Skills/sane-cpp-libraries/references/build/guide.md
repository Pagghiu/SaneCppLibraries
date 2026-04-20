# Sane Build

## Quick Start

- Use this guide when you need to describe or consume `SC::Build`, `SC-build.cpp`, or the native backend.
- Start with [build-definition-and-backends.md](references/build-definition-and-backends.md).

## Core Workflow

1. Inspect `Tools/SC-build.cpp` for real project setup patterns.
2. Inspect `Tests/Libraries/Build/BuildTest.cpp` for backend and generator coverage.
3. Model your workspace, projects, and configurations in `SC-build.cpp`.
4. Pick a backend: generated projects for IDE workflows or `SC::Build::Generator::Native` for direct builds and cross-target native-backend flows on macOS and Linux.

## Cross-Target Notes

- The native backend now models build machine, host machine, target machine, and runner explicitly instead of treating every build as host-native.
- Prefer friendly `build --target` profiles before raw overrides. Current public profiles include `linux-glibc-x86_64`, `linux-glibc-arm64`, `linux-musl-x86_64`, `linux-musl-arm64`, `windows-gnu-x86_64`, `windows-gnu-arm64`, `windows-msvc-x86_64`, and `windows-msvc-arm64`.
- `build --toolchain` is now public too. Current values include `default`, `host-default`, `clang`, `filc`, `gcc`, `msvc`, `clang-cl`, and `llvm-mingw`.
- Use raw `--triple` and `--sysroot` only as escape hatches; they intentionally override friendly profile defaults.
- Use `build run --runner <mode>` when the target is foreign. Current runner keywords are `auto`, `none`, `wine`, `qemu`, and `custom`.
- Foreign Linux targets can now run through `qemu-user` via `--runner qemu` or `--runner auto`; when that path is used the backend passes `-L <sysroot>` so dynamic Linux binaries resolve against the selected sysroot.
- On macOS and Linux, Windows GNU executables can be smoke-run through the shared Wine runner path. Wine launches are shaped through `cmd /c` with Windows-style target paths.
- On Linux arm64, that now includes targeted `windows-gnu-arm64` smokes through the packaged native ARM64 Wine runner.
- For Linux-target profiles on macOS and Windows, the native backend now auto-selects a packaged LLVM toolchain when explicit compiler paths are not provided. On macOS, the packaged Linux glibc and musl sysroots are now first-class too, so `linux-glibc-*` and `linux-musl-*` can compile without an explicit `--sysroot`.
- `SC-package install msvc` is the entry point for portable MSVC + Windows SDK acquisition. It now accepts `--import-directory <path>` and `--wine <path>` for imported layouts and custom runner wrappers.
- `SC-package install llvm` is now the packaged host-toolchain prerequisite for Linux-target native-backend work on non-Linux hosts.
- `SC-package install filc` is the new experimental compiler-first entry point for Linux Fil-C work. Use it together with `build ... --toolchain filc`; do not describe it as a public `linux-filc-*` target profile yet.
- `SCBuildTest` fixture runs now keep per-run projects, outputs, intermediates, and build cache isolated under `_Build/_Tests/...`, but reuse the repository `_Build/_PackagesCache` and `_Build/_Packages` roots so large packaged toolchains and runners are not re-downloaded for every fixture run.
- Once portable MSVC is installed, later native `windows-msvc-*` builds can reuse the wrapper path recorded in `sc-msvc-package.json` instead of requiring `SC_MSVC_WINE` again.
- Existing portable MSVC layouts can now repair missing metadata and wrapper scripts in place, and SDK version detection falls back from `Windows Kits/10/bin` to `Include` or `Lib` when the SDK tools directory is absent.
- Existing packaged Linux Wine runners now also repair their launcher scripts in place, so portable MSVC wrapper updates do not require deleting the cached runner package first.
- Portable MSVC caches are now host-specific, so shared macOS/Linux workspaces do not reuse the wrong recorded Wine wrapper path across hosts.
- On Linux arm64, the portable-MSVC install flow can auto-prefer generated `box64 + wine64` wrappers for the hosted x64 MSVC tools, while native `build run` now selects a separate ARM64 Wine runner for `windows-*-arm64` executables. Linux x64 console runs still prefer a sibling `wineconsole --backend=curses` wrapper when available, but Linux arm64 stays on plain `wine`.
- When Linux arm64 does not have a usable system `box64`, the packaged Wine runner can now resolve a maintained generic-arm `box64` package automatically; together with the packaged native ARM64 Wine runner, that path now validates clean native-backend `SCTest` compiles for both `windows-msvc-x86_64` and `windows-msvc-arm64`, plus targeted `BaseTest/new-delete` runs for both targets.

## Plugin Integration

- If the host executable will load plugins, configure exported Sane libraries on the host target.
- Prefer `SC::Build::Project::addExportLibraries(...)` for the minimal export set.
- Use `SC::Build::Project::addExportAllLibraries()` only when broad symbol exposure is acceptable.
- Remember that Linux plugin hosts also need `-rdynamic`.

## Common Pitfalls

- Do not treat `SC::Build` as a mandatory replacement for the user’s build system.
- Do not forget that the native backend is only available on macOS and Linux.
- Do not describe cross-target support as "run anything anywhere". Build support and run support still differ by host, target family, architecture, and available runner assets.
- Do not present Fil-C as a public support-matrix row yet; it is still an experimental Linux compiler track with compiler-first validation work in progress.
- Keep export-symbol decisions on the host executable, not on the plugin target.

## References

- [build-definition-and-backends.md](references/build-definition-and-backends.md)
- `Tools/SC-build.cpp`
- `Tests/Libraries/Build/BuildTest.cpp`
- `Documentation/Pages/Build.md`
