# TOOLS-0007 - Preserve Windows CI Coverage Across Independent Jobs

Status: Accepted
Date: 2026-10-25

## Context

Each Windows matrix job serially built single-file probes, core tests, experimental AsyncExecution tests,
and Release build-tool fixtures. Independent executables share no required build artifacts between these
groups. All lanes also waited for package warmup although only the build-tool fixtures consume the packages.

## Decision

Run core tests, auxiliary probes/AsyncExecution, and build-tool fixtures in independent checkouts. Retain
Windows 2022/2025 and both configurations for core and auxiliary work, and Release fixtures on both images.
Keep existing compiler backends, long-path settings, optimized builds, test arguments and OpenSSL requirements.
Only fixture jobs restore LLVM/sysroot packages and depend on the single cold-cache producer. Only auxiliary
jobs generate amalgamations and restore pinned stdexec headers; source-based core and fixture targets do not
consume these generated files.

Preserve the old `build (configuration, image, generator)` check names with lightweight aggregate matrix
jobs. They succeed only when every core, auxiliary and fixture matrix succeeds. Failed, cancelled or skipped
workloads cannot produce a green aggregate. The timing summary waits for all workload groups and aggregates.

## Consequences

Independent workloads can overlap and non-consumers avoid the package barrier. More jobs repeat checkout
and tool bootstrap, so queue time and total runner time must be measured alongside workflow elapsed time.
Existing required matrix checks continue to guard all original workloads.

## Confirmation

Compare expanded pre/post matrices and commands. Every original compile/test pair must retain its platform,
configuration, generator, environment and arguments. Verify all aggregate checks and workload matrices in
hosted CI, and inspect package restores to confirm they remain confined to consumers.
