# AsyncUV

Experimental source-compatible subset of the libuv C API implemented over `SC::Async`.
This is an evaluation project outside the supported library set. It is not ABI-compatible with libuv.
Core request state is stored inline in caller-owned handles and requests.

- [Supported API and limitations](Documentation/AsyncUVSupportedAPI.md)
- [Upstream test coverage](Documentation/AsyncUVTestCoverage.md)

Include `Code/uv.h` and compile `Code/SCAsyncUV.cpp` with the required SC libraries.
The standalone test target downloads the pinned libuv sources to compile selected upstream tests against AsyncUV.
The POSIX and Windows CI workflows build and run this target in Debug and Release.

From the repository root:

```sh
./SC.sh build configure SCAsyncUVTest
./SC.sh build compile SCAsyncUVTest Debug
./SC.sh build run SCAsyncUVTest Debug
./SC.sh build compile SCAsyncUVTest Release
./SC.sh build run SCAsyncUVTest Release
```
