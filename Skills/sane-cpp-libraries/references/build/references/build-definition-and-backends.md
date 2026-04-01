# Build Definition And Backends

Use this reference when a user needs to configure `SC::Build`, understand backends, or export Sane libraries for plugins.

## What This Library Does

- Describe builds imperatively in `SC-build.cpp`.
- Generate Xcode, Visual Studio, or Make projects.
- Run a native backend directly on macOS and Linux.

## Recommended Workflow

1. Read `Documentation/Pages/Build.md` for the overall model.
2. Inspect `Tools/SC-build.cpp` for real repository configuration.
3. Inspect `Tests/Libraries/Build/BuildTest.cpp` for generator and backend behavior.
4. Use `SC::Build::Definition`, `Workspace`, `Project`, and `Configuration` to model the build.
5. Select the backend that matches the user’s workflow.

## Native Backend Notes

- Select `SC::Build::Generator::Native` when the user wants direct builds without generated IDE files.
- The native backend is available on macOS and Linux.
- It launches compiler, linker, and archiver processes itself.

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
- Keep host export rules on the executable target.
