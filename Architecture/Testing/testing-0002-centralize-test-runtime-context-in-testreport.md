# TESTING-0002 - Centralize Test Runtime Context in TestReport

Status: Accepted
Date: 2026-07-04

## Context

Tests need shared runtime context: output, selected test and section filters, repository root paths, executable paths, optional slow-test flags, port offset mapping, and memory leak reporting. Spreading that setup across individual tests would duplicate platform path logic and make agent-driven test runs harder to reason about.

## Decision

`TestReport` is the central runtime context for test execution. It parses supported command-line options and environment settings, resolves the library root and executable/application paths, owns test counters and current section state, maps ports, and provides the global memory report hook.

## Consequences

Individual `TestCase` implementations stay small and receive a single report object. Test runtime behavior is consistent across libraries and platforms. `TestReport` is larger than a minimal assertion counter, so new runtime features should be added there only when they are broadly useful across tests.

## Confirmation

A change preserves this decision when test cases continue receiving `TestReport&`, command-line filtering and path resolution remain centralized, port mapping and optional-test state are read through `TestReport`, and individual library tests do not duplicate repository root discovery or global test accounting.

## Related

- [Testing public API](../../Libraries/Testing/Testing.h)
- [Testing implementation](../../Libraries/Testing/Testing.cpp)
- [SCTest main](../../Tests/SCTest/SCTest.cpp)
- [TESTING-0001 - Keep Testing as a low-dependency library with injected output](testing-0001-keep-testing-as-a-low-dependency-library-with-injected-output.md)
