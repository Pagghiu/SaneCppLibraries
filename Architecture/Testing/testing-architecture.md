# Testing Architecture

## Purpose

Testing provides the small test framework used by Sane C++ Libraries. It records expectations, groups tests into sections, centralizes runtime test context, and keeps the repository test executable predictable for local, CI, and agent-driven runs.

## Architectural Shape

Testing is built around `TestReport` and `TestCase`. `TestReport` owns output, command-line options, paths, counters, port mapping, optional-test state, and memory-report integration. `TestCase` is a lightweight base whose constructor runs sectioned checks and reports expectations through the shared report.

Output is injected through `TestReport::IOutput`, so the library does not depend on the concrete Console implementation. Paths are stored in fixed `StringPath` buffers, and runtime path discovery lives in `Testing.cpp` behind platform-specific implementation code.

## Boundaries

Testing owns the test framework, report lifecycle, filtering options, section execution, portable path discovery for tests, and test-run coordination features such as port offsets and optional tests. It does not own test discovery yet, benchmark orchestration, CI workflow policy, memory allocator implementation, console implementation, or individual library test behavior.

Tests may depend on higher-level libraries. The Testing library itself must remain low-dependency because every test and many examples can include it.

## Similarities With Other Libraries

Testing follows the same independent-library rule as production libraries. It uses Common primitives, explicit status reporting, no exceptions, and platform-specific code hidden in `.cpp` implementation. It also follows the agentic workflow emphasis on reproducible commands and focused test selection.

## Differences From Other Libraries

Testing is allowed to be developer-facing and process-oriented in ways production libraries are not: it parses test command-line flags, prints summaries, can break into the debugger, and can terminate release test runs after a failure. Unlike application libraries, its public API intentionally exposes test-run controls.

## Inspirations

The evidenced inspirations are the project's own test suite needs: fast focused runs, section filtering, quiet output, memory reporting, parallel-friendly networking tests, and optional slow tests. The 2026 testing/build notes explicitly call out reducing conflicts during parallel runs.

## Anti-Inspirations

Testing is not a heavyweight unit-test framework, not a template-test generator, and not currently an automatic test discovery system. Inference: it intentionally avoids framework-style global registration and allocation-heavy discovery so the harness remains simple and low-dependency until discovery is deliberately designed.

## Architectural Choices

Keep `TestReport` as the central runtime context. New cross-cutting test-run state should go there rather than being copied into individual tests.

Keep output injected. Do not make Testing depend on Console, Strings, Memory, STL streams, or formatting libraries.

Network tests must use `report.mapPort` for bindable ports. Slow or externally expensive tests must remain opt-in through `runAllTests`, explicit selection, or a later documented mechanism.

## Explicitly Excluded Targets

- Hidden dependencies on Memory, Strings, Containers, Console, or STL.
- Automatic test discovery without a new accepted design.
- A general mocking framework or benchmark framework.
- Library-specific test fixtures inside the Testing library.
- Hard-coded final network ports in tests that can run in parallel.

## Sources

- [Testing documentation](../../Documentation/Libraries/Testing.md)
- [Testing public API](../../Libraries/Testing/Testing.h)
- [Testing implementation](../../Libraries/Testing/Testing.cpp)
- [SCTest main](../../Tests/SCTest/SCTest.cpp)
- [Agent guidelines: testing](../../AGENTS.md)
- [2026-02 Sane C++ Libraries update: Testing and Build](../../../SC-website/pagghiu.github.io-source/content/blog/2026-02-28-SaneCppLibrariesUpdate.md)
- [2025-09 Sane C++ Libraries update: dependency cleanup](../../../SC-website/pagghiu.github.io-source/content/blog/2025-09-30-SaneCppLibrariesUpdate.md)
- [SC-0003 - Keep Libraries Independently Consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0013 - Maintain Agentic Development As the Primary Contribution Workflow](../Global/sc-0013-maintain-agentic-development-as-the-primary-contribution-workflow.md)
- [TESTING-0001 - Keep Testing As a Low-Dependency Library with Injected Output](testing-0001-keep-testing-as-a-low-dependency-library-with-injected-output.md)
- [TESTING-0002 - Centralize Test Runtime Context in TestReport](testing-0002-centralize-test-runtime-context-in-testreport.md)
- [TESTING-0003 - Make Local SCTest Runs Parallel-Friendly with Opt-In Slow Tests and Port Offsets](testing-0003-make-local-sctest-runs-parallel-friendly-with-opt-in-slow-tests-and-port-offsets.md)

## Decision Log

- [TESTING-0001 - Keep Testing as a low-dependency library with injected output](testing-0001-keep-testing-as-a-low-dependency-library-with-injected-output.md)
- [TESTING-0002 - Centralize test runtime context in TestReport](testing-0002-centralize-test-runtime-context-in-testreport.md)
- [TESTING-0003 - Make local SCTest runs parallel-friendly with opt-in slow tests and port offsets](testing-0003-make-local-sctest-runs-parallel-friendly-with-opt-in-slow-tests-and-port-offsets.md)
