# Native BSD validation

Use a guest-local checkout on NetBSD 11 with GCC, FreeBSD 15.1 with Clang, or OpenBSD 8.0 amd64
with base Clang. The BSD GitHub Actions workflow runs native validation on x86_64 guests for all three
systems and runs prebuilt SCTest on NetBSD and FreeBSD ARM64 guests.
The complete run needs Python 3 for amalgamation, Git and package inputs for the build tests,
and libcurl visible to the dynamic loader. NetBSD and FreeBSD also require OpenSSL 3. If Git is installed outside
the base system, put it on PATH and configure its runtime library search path before starting.

```sh
sh Tests/SCBSDTest/run.sh
SC_TEST_PORT_OFFSET=200 sh Tests/SCBSDTest/run.sh
sh Tests/SCBSDTest/run.sh Debug --test SocketTest
sh Tests/SCBSDTest/run.sh Release --test FibersTest --test-section 'incremental stack faults'
```

The default run regenerates the single-file sources, configures projects, compiles and runs
SCTest with `--all-tests` in Debug and Release, and builds the single-file targets in both
configurations.
NetBSD and FreeBSD require the OpenSSL 3 provider and incremental fiber stack tests to run. OpenBSD
uses the existing native cryptography capability checks and fixed fiber stacks. The complete run may fetch packages
on a cold cache. An explicit configuration runs the selected SCTest cases. Each invocation
uses temporary test storage. Use separate checkouts and distinct port offsets for concurrent
validation on the same guest.

For a guest without Python, run `python3 Support/SingleFileLibs/python/amalgamate_single_file_libs.py`
in a matching checkout and copy `_Build/_SingleFileLibraries` and `_Build/_SingleFileLibrariesTest`
into the guest checkout. Then run `sh Tests/SCBSDTest/run.sh prebuilt-headers`.

See the [BSD support matrix](../../Documentation/Pages/Build.md#bsd-support-matrix) for the tested features.

The default command runs the shared suite. The TLS and HTTP/2 fixture needs its own server
and environment. To test missing or incomplete libcurl installations and versioned library
fallback, run `sh Tests/Libraries/HttpClient/run-libcurl-runtime-fixture.sh`.

For ARM64 CI, Ubuntu x86_64 builds Debug and Release `SCTest` against the matching BSD release
sysroot. The workflow copies the source checkout and binaries into the guest. Keep each executable
under its usual `_Build/_Outputs` configuration directory so child tests can locate repository fixtures.

```sh
sh Tests/SCBSDTest/run.sh prebuilt _Build/_Outputs/freebsd-arm64-Native-clang-Release/SCTest --all-tests
```

The prebuilt mode runs the suite without recompiling it. Plugin tests still compile their small
fixtures with the guest's base compiler.

OpenBSD CI installs Git and curl through `pkg_add` and caches the prepared guest image. It runs the
same shared suite and generated targets in both configurations. TLS/HTTP2 integration uses a separate
fixture and remains outside the OpenBSD CI coverage.
