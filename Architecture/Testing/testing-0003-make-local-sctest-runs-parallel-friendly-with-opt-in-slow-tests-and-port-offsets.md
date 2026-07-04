# TESTING-0003 - Make Local SCTest Runs Parallel-Friendly with Opt-In Slow Tests and Port Offsets

Status: Accepted
Date: 2026-07-04

## Context

Agents and maintainers often run tests across multiple worktrees, configurations, or platforms at the same time. Network tests can conflict on fixed ports, and slow or package-downloading tests can make ordinary local runs expensive or flaky.

## Decision

Testing supports parallel-friendly local runs through `--port-offset` or `SC_TEST_PORT_OFFSET`, exposed as `TestReport::mapPort`, and through `--all-tests`, exposed as `TestReport::runAllTests`, for optional slow tests that should not run by default locally.

## Consequences

Tests that bind ports must map their base ports through `TestReport` instead of hard-coding final ports. Slow tests can remain in SCTest without penalizing every local run. CI and explicit local commands can still enable the full suite with `--all-tests`.

## Confirmation

A change preserves this decision when network tests use `report.mapPort`, optional slow tests check `report.runAllTests` or equivalent explicit selection, `--port-offset` and `SC_TEST_PORT_OFFSET` continue to work, and local default SCTest runs do not perform opt-in slow/package tests.

## Related

- [Testing public API](../../Libraries/Testing/Testing.h)
- [Testing command-line parsing](../../Libraries/Testing/Testing.cpp)
- [Agent testing guidelines](../../AGENTS.md)
- [SC-0013 - Maintain agentic development as the primary contribution workflow](../Global/sc-0013-maintain-agentic-development-as-the-primary-contribution-workflow.md)
