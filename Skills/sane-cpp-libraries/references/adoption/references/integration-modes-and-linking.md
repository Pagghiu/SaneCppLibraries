# Integration Modes And Linking

## Choose An Adoption Path

### Prefer single-file libraries when

- The user wants one library or a very small subset.
- The user wants to avoid vendoring the full repo.
- The user is comfortable with one implementation-defining translation unit per amalgamated header.

### Prefer full-repo integration when

- The user wants several libraries.
- The user wants tests and examples as references.
- The user may later use `SC::Build`, `SC::Tools`, or plugin workflows.
- The user wants public headers directly from `Libraries/<Library>`.

## Single-File Path

Source references:

- `README.md`
- `Documentation/Pages/BuildingUser.md`
- `Documentation/Pages/SingleFileLibs.md`

Recommended steps:

1. Obtain the amalgamated header from the GitHub release or assemble it from the repo.
2. Include `SaneCpp<Library>.h` where needed.
3. In exactly one `.cpp` file, define `SANE_CPP_IMPLEMENTATION` before including the same header.
4. If the project intentionally wants strict no-stdlib mode, define `SC_DISABLE_STD_CPP=1`.
5. Add any required system libraries or frameworks for the target platform.

Generation paths to mention when the user wants current-main artifacts:

- `python3 Support/SingleFileLibs/python/amalgamate_single_file_libs.py`
- `node Support/SingleFileLibs/javascript/cli.js --repo-root . --ref HEAD --all --out _Build/_SingleFileLibrariesJS`
- Browser assembly via the Single File Libraries page

## Full-Repo Path

Source references:

- `README.md`
- `Documentation/Pages/BuildingUser.md`

Recommended steps:

1. Vendor or clone the repo into the user's project.
2. Add `SC.cpp` to the user's build.
3. Include only public headers from `Libraries/<Library>/*.h`.
4. Do not include `Internal` or `Tests` headers as public dependencies.
5. Add platform link requirements.

## Platform Link Requirements

### macOS

- Link `CoreFoundation.framework`
- Link `CoreServices.framework`

### Linux

- Link `-ldl`
- Link `-lpthread`

### Windows

- No extra external library step is normally needed for the documented core setup.
- The repo documents implicit pragma-linked dependencies including `Ws2_32.lib`, `ntdll.lib`, `Rstrtmgr.lib`, and `DbgHelp.lib`.

## Standard Library Guidance

- Default posture: Sane C++ Libraries can be included from normal C++ projects without a stdlib opt-in macro.
- SC library code still avoids STL containers, exceptions, hidden allocations, and C++ runtime dependencies where practical.
- SC-build avoids C++ runtime linkage by default; use `project.saneCpp.linkStdCpp = true` only for targets that need STL
  runtime features. Non-SC-build integrations can define `SC_AVOID_STD_CPP_LINK=1` when passing no-C++-runtime linker
  flags manually.
- If the user wants the strict historical no-header no-stdlib posture, tell them to define `SC_DISABLE_STD_CPP=1` and
  use matching build flags or `project.saneCpp.disableStdCpp = true` in SC-build.
- Do not imply that users must rewrite their whole application in Sane style on day one.

## Plugin Caveat

If the user integrates the Plugin library without `SC::Build`, the host executable must export the required Sane libraries:

- Add `SC_EXPORT_LIBRARY_<LIBRARY>=1` defines to the host executable build.
- On Linux, also add `-rdynamic`.

Route deeper plugin setup to `plugin-build` or `plugin`.
