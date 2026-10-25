# Agent Guidelines for AsyncUV

This project is a source-compatible subset of the libuv C API, implemented as a wrapper around the `SC::Async` library.

## Project Structure

- `Code/`: Implementation of the libuv wrapper.
  - `uv.h`: Header file declaring the supported libuv API.
  - `SCAsyncUV.cpp`: Main implementation file.
- `Tests/`: Test suite.
  - `SCAsyncUVTest.cpp`: Standalone test runner with a `main` function.
- `Documentation/`:
  - `AsyncUVSupportedAPI.md`: List of currently supported types/functions.
  - `AsyncUVTestCoverage.md`: Progress tracking for upstream libuv tests.

## Essential Rules

1. **Source Compatibility**: Maintain the `uv_*` naming for supported types and functions.
2. **Zero Allocation**: Core request flows (like `uv_write` or `uv_udp_send`) must store state inline within the request object to avoid runtime heap allocations. Do not introduce heap allocations without explicit authorization (the only one currently authorized is for `uv_thread_create`).
3. **Internal Logic**: Use `SC::Async` as the underlying engine. Propose a plan before implementing any large change to `SC::Async` that is not a bugfix or a trivial extension of the existing API.
4. **Header Cleanliness**: Keep `uv.h` as clean as possible.

## Commands

```bash
# Regenerate build configuration (after adding files or changing SC-Build.cpp)
./SC.sh build configure SCAsyncUVTest

# Compile the standalone test project
./SC.sh build compile SCAsyncUVTest Debug

# Run the tests
./SC.sh build run SCAsyncUVTest Debug
```

## Testing Protocol

- New features or bug fixes should be verified by enabling or adding relevant tests to `SCAsyncUVTest`.
- Always verify that the tests pass across available platforms.
- Ensure no regressions are introduced in the implementation of the `uv_*` C API.
- If you make any changes to Sane C++ Libraries, the SCTest suite should be run on macOS, Windows and Linux and they must be committed separately.

## Further Reading
- [Documentation/AsyncUVSupportedAPI.md](Documentation/AsyncUVSupportedAPI.md)
- [Documentation/AsyncUVTestCoverage.md](Documentation/AsyncUVTestCoverage.md)
