@page library_testing Testing

@brief A small, explicit test runner for SC-style result checking and sectioned tests

[TOC]

[SaneCppTesting.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppTesting.h) is the
framework used by the repository's `SCTest` executable. It is a fit when a project wants a test harness with the same
constraints and vocabulary as the rest of Sane C++ Libraries: no STL, no exceptions, a caller-selected output sink,
and direct handling of `SC::Result`.

It is intentionally not a general-purpose replacement for Catch2, GoogleTest, or doctest. There is no test discovery,
fixture registry, parameterized-test machinery, matcher library, or IDE protocol. A test is ordinary C++ code, invoked
explicitly from the program's `main`, and grouped with `if (test_section(...))` branches. That makes control flow easy to
follow and keeps the framework small, at the cost of more manual wiring and fewer diagnostics.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Testing.svg)


# The execution model

`TestReport` owns the run-level state: the output interface, command-line selection, resolved executable/source paths,
aggregate counts, optional-test mode, and network port offset. `TestCase` owns the counts for one named test. Its
constructor starts reporting and its destructor folds the result into the report, so a test object must remain alive
until all of its sections have run.

Derived test cases normally execute their checks directly in their constructor. `test_section()` is not a nested test
object: it decides whether the following ordinary C++ block should execute and labels its expectations. `SC_TEST_EXPECT`
accepts either a Boolean expression or an `SC::Result`; for a failed `Result`, the result message becomes the detailed
error. Successful expectations are counted but only section summaries and failures are printed.

The repository's Console test is representative source, not a synthetic API catalogue:

@snippet Tests/Libraries/Strings/ConsoleTest.cpp testingSnippet

The final `runConsoleTest()` function is significant. `SCTest.cpp` declares and calls functions like this explicitly;
adding a test source file alone does not make it run. This is useful in freestanding or unusual build environments where
static registration is undesirable, but forgetting either the declaration or call silently leaves a test out.

# Building a runner

A runner supplies an implementation of `TestReport::IOutput`. `TestReport::Output<ConsoleType>` adapts an existing
console-like object without taking ownership of it. The adapter and console must therefore outlive the report. The
report parses the test arguments during construction, resolves useful paths, and emits its final summary during
destruction; return `getTestReturnCode()` only after all test cases have been destroyed.

The repository runner in `Tests/SCTest/SCTest.cpp` constructs a `Console`, wraps it in
`TestReport::Output<Console>`, constructs the report from `argc` and `argv`, checks `hasStartupFailure()`, calls every
registered `run...Test(report)` function, and finally returns `getTestReturnCode()`. Platform services used by the
tests—such as the global allocator or socket networking—are initialized by that application. Testing does not
initialize neighboring libraries on their behalf.

# Selecting work from the command line

Arguments after the build tool's `--` separator belong to the test executable:

@code{.sh}
./SC.sh build run SCTest Debug -- --test "ConsoleTest" --test-section "print"
@endcode

The runner recognizes:

- `--test <name>` to run one explicitly registered test case;
- `--test-section <name>` to run matching sections;
- `--all-tests` to set `TestReport::runAllTests`, which individual tests may inspect to opt into optional or slow work;
- `--quiet` to suppress normal summaries, while failures can still be reported;
- `--port-offset <n>`, or `SC_TEST_PORT_OFFSET`, to keep parallel network tests from sharing ports;
- `--library-root <path>` to override source-root discovery.

Sections declared with `Execute::OnlyExplicit` run only when their exact name is selected with `--test-section`. This is
the mechanism used for disruptive, environment-sensitive, or especially slow checks that should not join the normal
suite merely because `--all-tests` was supplied.

`mapPort(basePort)` applies the configured offset and is the expected bridge between Testing and Socket/Http tests.
Each test still chooses its base ports and owns its network lifetime; Testing only provides deterministic remapping.

# Failure and lifetime behavior

By default `debugBreakOnFailedTest` is enabled, so `SC_TEST_EXPECT` can stop in an attached debugger. The runner may
disable it for unattended execution. `abortOnFirstFailedTest` defaults to true, but the current implementation exits
early only in Release builds after a failed test case finishes; Debug retains debugger-oriented behavior. Both settings
are public run policy, not per-expectation annotations.

Testing does not allocate merely to register cases, sections, or expectations. Names and expressions are borrowed
`StringSpan` values and must remain valid while they are reported; expectation expressions and section names are also
required to be null-terminated. The report stores resolved paths in bounded `StringPath` buffers rather than growing
heap strings. The supplied output adapter also borrows its console. Allocation performed by the code under test is
separate, and `runGlobalMemoryReport()` can compare allocator counters when the host exposes compatible statistics; it
is a counter check, not leak tracing or allocation interception.

Because reporting is tied to destructors, avoid terminating the process from inside a test if the summary matters.
Likewise, asynchronous tests must drive their work to completion and close resources before the test object is
destroyed; the framework itself is synchronous and does not provide an event loop, timeout scheduler, or async fixture
lifecycle.

# Fit and neighboring libraries

Testing is strongest as an in-repository harness for SC libraries and small projects that value transparent control
flow, portable builds, and `Result`-aware checks more than rich test-framework features. The single-header download is
useful where bringing in a conventional C++ test dependency is not acceptable.

Its boundaries are deliberate:

- **Common fragments** provide the dependency-free substrate used internally; they are implementation material, not a
  public library dependency.
- **Strings** commonly supplies `Console`, which can be adapted to `IOutput`, but any compatible output implementation
  can be used.
- **Memory** can supply allocator statistics to `runGlobalMemoryReport()`; Testing does not own or replace the allocator.
- **Socket and Http** use `mapPort()` to make parallel runs coexist; they remain responsible for setup and cleanup.
- **Build** compiles and launches `SCTest` and forwards arguments, but test selection itself belongs to Testing.

Choose a larger framework if automatic discovery, data-driven cases, expressive value printers, death tests, mocking,
machine-readable reports, or first-class IDE integration are requirements. Testing's minimalism is an operational
constraint, not a claim that those facilities are unnecessary.

# Status and direction

🟨 **MVP.** The framework is actively useful across the SC suite, including focused sections, an optional-work policy
flag consumed by tests, parallel port mapping, startup path reporting, and aggregate memory counters. Its main usability
limitation remains explicit registration. Test discovery and IDE integration are plausible future work;
template/parameterized tests are currently unplanned rather than partially supported.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Testing`.
Single File counts
`SaneCppTesting.h`.
Standalone counts `SaneCppTestingStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 155		| 791		| 946	|
| Single File | 1178		| 1382		| 2560	|
| Standalone  | 1178		| 1382		| 2560	|
