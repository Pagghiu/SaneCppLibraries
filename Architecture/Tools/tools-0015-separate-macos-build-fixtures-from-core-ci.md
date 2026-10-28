# TOOLS-0015 - Separate macOS Build Fixtures from Core CI

Status: Accepted for measured trial
Date: 2026-10-26

## Context

macOS Release remains the Posix critical path: at 740d98833 it executed for 391 seconds, including
79 seconds of build-fixture tests, 13 seconds compiling them, and cross-toolchain package setup. Core
Xcode compilation and execution, stdexec and standalone tests also run in this job. Debug lacks these
fixtures and compiles only the native architecture. Controlled profiles found no abrupt optimization
cliff in the sampled core/auxiliary translation units. Constructor scenario outlining changed the large
stdexec test TU only 21.2 to 20.8 seconds, so that source change was rejected.

Running fixture tests alongside Xcode compilation on one runner caused CPU contention that largely
erased the elapsed benefit. A separate runner can overlap those workloads.

## Decision

Move macOS Release SCBuildTest compilation/execution, conditional real-QEMU checks, and their existing
LLVM/llvm-mingw/sysroot restore, preparation, verification and save steps to one additional macOS job.
Keep Linux Release fixture commands in their original matrix lane. Retain all core Debug/Release lanes,
universal arm64+x86_64 Xcode compilation, test sections, compiler/sanitizer settings, cache identities,
package versions and timeout budgets. Package setup moves instead of being duplicated in core.

Rename the workload matrix to core and preserve the original build matrix check names as Ubuntu gates
over core and macos-build-fixtures. Either workload failure must fail those gates. Keep scheduled core
work skipped as before, and leave both Fil-C lanes and their schedule unchanged. Give fixture crash
artifacts distinct names and include the fixture executable.

## Consequences

This adds one macOS runner and four small Ubuntu gate jobs, with additional checkout/bootstrap and gate
work. Queue delay may outweigh parallelism; the earlier 603-second macOS queue is relevant evidence.
There is no claim of reduced compiler CPU work. The independent Fil-C lanes limit the whole-workflow
saving after macOS stops dominating.

## Confirmation

A structural comparison must retain every original command/environment/cache contract, partition the
macOS and Linux fixture commands exactly once, preserve the required check matrix, and retain all other
jobs/settings. The full public matrix must pass. Compare original section names, job execution, queue
delay, total runner-seconds and workflow elapsed against 740d98833. Reject or revise the split if extra
queue/bootstrap cost prevents a useful latency improvement.
