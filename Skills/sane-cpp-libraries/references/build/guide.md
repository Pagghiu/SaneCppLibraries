# Sane Build

## Quick Start

- Use this guide when you need to describe or consume `SC::Build`, `SC-build.cpp`, or the native backend.
- Start with [build-definition-and-backends.md](references/build-definition-and-backends.md).

## Core Workflow

1. Inspect `Tools/SC-build.cpp` for real project setup patterns.
2. Inspect `Tests/Libraries/Build/BuildTest.cpp` for backend and generator coverage.
3. Model your workspace, projects, and configurations in `SC-build.cpp`.
4. Pick a backend: generated projects for IDE workflows or `SC::Build::Generator::Native` for direct builds on macOS and Linux.

## Plugin Integration

- If the host executable will load plugins, configure exported Sane libraries on the host target.
- Prefer `SC::Build::Project::addExportLibraries(...)` for the minimal export set.
- Use `SC::Build::Project::addExportAllLibraries()` only when broad symbol exposure is acceptable.
- Remember that Linux plugin hosts also need `-rdynamic`.

## Common Pitfalls

- Do not treat `SC::Build` as a mandatory replacement for the user’s build system.
- Do not forget that the native backend is only available on macOS and Linux.
- Keep export-symbol decisions on the host executable, not on the plugin target.

## References

- [build-definition-and-backends.md](references/build-definition-and-backends.md)
- `Tools/SC-build.cpp`
- `Tests/Libraries/Build/BuildTest.cpp`
- `Documentation/Pages/Build.md`
