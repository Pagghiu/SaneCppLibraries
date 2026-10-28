# TOOLS-0016 - Partition Fil-C Core and Support Tool Tests

Status: Accepted for measured trial
Date: 2026-10-27

## Context

After separating macOS fixtures, Fil-C x64 is the Posix critical path at 330 seconds,
including 240 seconds compiling SCTest. Isolated ARM64 Release profiles find SC-build
at 51.8 seconds, SC-package at 37.2 and SupportToolsTest at 25.0. Backend optimization
and instrumentation dominate; no single function optimizer event dominates. Scratch
noinline tool-helper probes did not establish a useful overall improvement.

Native log completion markers can be delayed until the final event-loop drain. Their
start/completion gaps are upper bounds, not reliable individual compiler durations.

## Decision

Keep the complete SCTest target as the development default. Add explicit SCTestCore
and SCTestTools targets with the same shared configuration, library inventory, exports,
compiler options and source mode. Use the existing SCTest main and preserve startup,
networking and final global-memory checks in each. Core runs all existing library and
support tests except SupportToolsTest; tools runs SupportToolsTest and compiles all
existing public tool-header checks and tool implementations. Their union retains the
original source and test inventory. No test body or optimization/instrumentation setting
changes. Shared library sources are deliberately compiled separately for each target.

Trial both targets on separate Fil-C runners for each existing architecture. Restore and verify the existing TLS runtime package caches in both partitions because
SC-build prepares those libraries for every Fil-C executable. Keep the real TLS fixture
on core; tools retains the same heavy-test environment policy. Preserve the original required Fil-C check names through
gates that fail on any workload failure, cancellation or skip. Keep coroutine checks on
core and retain scheduled coverage. The TLS wrapper accepts only SCTest or SCTestCore
through SC_FILC_TEST_PROJECT and defaults to the complete original target.

## Consequences and confirmation

This adds two Linux runners and two small gate jobs. Shared library compilation and
compiler package restore/bootstrap are duplicated. Compare elapsed time, queue delay
and total runner work rather than claiming less compiler work. No compiler-cache
expansion or cross-target object reuse is part of this trial.

Check the source/flag inventory partition, original and partitioned Debug/Release suites
on available platforms, standalone compilation, and full public Fil-C TLS runs on both
architectures. Retain only if the measured latency improvement justifies duplicated work.
