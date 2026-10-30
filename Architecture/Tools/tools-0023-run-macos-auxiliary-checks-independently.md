# TOOLS-0023 - Run macOS Auxiliary Checks Independently

Status: Accepted
Date: 2026-10-29

## Context

The default-reuse CI run spends 307 seconds in macOS Release. Independent
AsyncExecution compilation takes 53 seconds and single-file compilation takes
43 seconds before the main Xcode build. These checks do not consume SCTest's
outputs. Windows already groups analogous checks in an auxiliary job.

## Decision

Run macOS Debug and Release AsyncExecution/stdexec and single-file checks in
independent auxiliary jobs, with the same runner, configuration, pinned stdexec
revision, generation/configuration/build commands and runtime checks. Keep Linux
commands in the existing core jobs. Preserve the main native/Xcode, sanitizer,
interop and build-fixture coverage and flags. Both auxiliary configurations remain
required workflow results. Include the auxiliary job in the existing required
`build` aggregators so either auxiliary configuration failing rejects those checks.
Do not add a dependency barrier to the core jobs.

## Consequences

The main macOS job can begin its Xcode work sooner. Extra checkout and build-tool
startup cost some runner work; measure that overhead and actual job completion
before retaining the split. Retain crash diagnostics for the moved executable.
This change rearranges work and does not reduce the amount of compilation covered.

## Confirmation

Audit the per-platform/configuration command inventory and stdexec revision, then
verify both auxiliary jobs and all existing workflows. Compare runner execution
and phase times separately from queue changes. Reject the split if setup/queue
costs erase its substantial predicted critical-path improvement.
