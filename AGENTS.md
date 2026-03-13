# Agent Guidelines for Sane C++ Libraries

> **Quick context:** Platform abstraction libraries (macOS/Windows/Linux) with **NO STL/exceptions/RTTI** and **NO allocations** in library code. See [Principles](Documentation/Pages/Principles.md) and [README.md](README.md).

## Essential Rules

1. **No dynamic allocation** in library code — APIs take caller-provided `Span`/`StringView` buffers
2. **No STL** — use `SC::` types (`Vector`, `String`, `Function`, etc.)
3. **No exceptions** — use `SC_TRY(result)` for error propagation; functions return `Result`
4. **No system headers in public `.h`** — only in `.cpp` or internal files
5. **No dependencies** — neither external nor between libraries (check existing patterns)

## Commands

```bash
# Regenerate projects (when project files are missing, after adding files or modifying SC-Build.cpp)
./SC.sh build configure SCTest

# Build all tests
./SC.sh build compile SCTest Debug

# Verify SingleFileLibs still compile (catches forbidden inter-library dependencies)
./SC.sh build compile "SCSingleFileLibs:" Release

# Run all tests (ALWAYS "build compile" before running tests!!)
./SC.sh build run SCTest Debug

# Run full suite including BuildTest (downloads package repos if cache is empty)
./SC.sh build run SCTest Debug -- --all-tests

# Run specific test (saves context)
./SC.sh build run SCTest Debug -- --test "TestName"

# Run specific test section within a test - if (test_section("section name"))
./SC.sh build run SCTest Debug -- --test "TestName" --test-section "section name"

# Format code (run before finishing)
./SC.sh format execute

# Build documentation
./SC.sh build documentation
```
NOTE: build is the name of the TOOL, but is the word AFTER it (the ACTION) that determines what the command is actually doing.
`./SC.sh build run SCTest` doesn't "compile" SCTest, but it just runs the previously built executable.

## Code Style

Follow [Coding Style](Documentation/Pages/CodingStyle.md) and match surrounding code, and before committing clang format must be used.

**Key points:**
- Use `and`/`or`/`not` instead of `&&`/`||`/`!`
- When clang-format would introduce very wide alignment padding between adjacent declarations, add a blank line to split them into smaller groups instead of keeping columns aligned with large runs of spaces.
- Format with: `./SC.sh format execute`
- Commit message: see "Commit message format" from CONTRIBUTING.md

## Testing

- Tests live in `Tests/Libraries/*`
- Tests should be **fast** — if stuck, likely async resources aren't being closed
- Always run all tests in **both Debug and Release** before completing
- Run tests on Linux, macOS and Windows if you can access the required VM or it's the local host OS
- Never invoke executables like AsyncWebServer directly, always use ./SC.sh build run ... (unless you need to debug directly in lldb / gdb)
- `SCTest` can run in parallel across worktrees/configurations; isolate network ports per process when doing so.
- Use `--port-offset <N>` (for example `./SC.sh build run SCTest Debug -- --port-offset 200`) or `SC_TEST_PORT_OFFSET=<N>`.
- `BuildTest` is skipped by default for local runs; enable it with `--all-tests` (CI already runs SCTest with `--all-tests`).
- Make sure not to break amalgamated single file libs:
    - amalgamate: `python3 Support/SingleFileLibs/python/amalgamate_single_file_libs.py`
    - compile: `./SC.sh build compile "SCSingleFileLibs:" Release`
- When debugging async workflows add printf to reconstruct the sequence of calls
- Remove debug log after user confirmation that everything is correct
- Tests use --nostdinc so including <new>, <cstdio> or anything similar will fail

## Developing

- **Naming**: Check existing type names before defining new ones — the `SC::` namespace is flat, so collisions with other libraries cause redefinition errors
- **Obj-C/C++ files (`.m`,`.mm`)**: Inside libraries use `objc_msgSend` to enable compiling everything as single C++ unit. Using `.mm` in examples or tests is acceptable.
- **Test registration**: New tests must be declared and invoked in `Tests/SCTest/SCTest.cpp` — both a forward declaration and a call in `main()`
- **Port allocation in tests**: Use `report.mapPort(basePort)` to avoid port conflicts between parallel test runs

## Common Pitfalls

| Error | Cause | Fix |
|-------|-------|-----|
| "Lambda too big" | `SC::Function` has fixed capture size | Capture a pointer to an outer object instead |
| Test hangs | Async resources not closed | Check refcount; add printf to trace callbacks |
| Port already in use | Previous hung test | Kill the stuck process first |
| Error including <cstdio> | Tests are run with --nostdinc | use <stdio.h> |
| Redefinition of type | Name collision across libraries | Use unique prefixes per library (e.g. `NewLibrary*`) |
| Duplicate symbols at link | `.mm` + `.cpp` both define same functions | Guard `.cpp` with `#if !SC_PLATFORM_APPLE` |
| `offsetof` on non-standard-layout | Compiler `-Werror` rejects it | Access `storage` member directly instead |
| `no matching operator new` | Raw placement new needs `<new>` | Do NOT use raw `new (ptr) T()` which requires `<new>` header (forbidden by `--nostdinc`) - Use `SC::placementNew()` instead |

## Memory & APIs

- Use `GrowableBuffer<T>` / `IGrowableBuffer` for unbounded growth
- Pointers = optional (can be `nullptr`); references = required
- Handle "insufficient memory" errors — return them to caller via `Result`
- Use `MakeDeferred` for non-RAII cleanup

## Project Layout

| Path | Description |
|------|-------------|
| `Libraries/*` | Core libraries |
| `Tests/Libraries/*` | Tests per library |
| `Examples/SCExample` | GUI example with async integration |
| `Tools/SC-Build.cpp` | Build configuration |

## Further Reading

- [CONTRIBUTING.md](CONTRIBUTING.md) — commit format, PR guidelines, naming
- [Coding Style](https://pagghiu.github.io/SaneCppLibraries/page_coding_style.html)
- [Building (contributor)](https://pagghiu.github.io/SaneCppLibraries/page_building_contributor.html)
