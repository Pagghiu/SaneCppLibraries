# TOOLS-0006 - Bound Visual Studio Solution Parallelism

Status: Accepted
Date: 2026-10-25

## Context

Visual Studio solution compilation previously invoked MSBuild without a project-worker limit, which defaults
to one worker. The single-file workspace contains many independent one-source projects, so compiler `/MP`
alone cannot parallelize them. Clean Windows VM Release rebuilds measured 110.1 seconds with one project
worker, 52.8 seconds with two, and 52.5 seconds with four. Additional workers did not improve that sample.

## Decision

For Visual Studio solution compile actions, use up to two project workers automatically, bounded by the
reported processor count. Honor an explicit `ExecutionOptions::maxParallelJobs` as the project-worker limit.
Divide the processor budget across the selected project workers through `CL_MPCount`, with at least one
compiler worker per project. Disable MSBuild node reuse so solution workers do not retain workspace paths
after completion. Keep single-project compilation, run/print actions, configurations and optimization flags
unchanged. Native-backend limits retain their existing meaning.

## Consequences

Independent solution projects can build together without multiplying an unbounded compiler `/MP` setting
by the number of MSBuild workers. The automatic default favors the smaller measured worker count; callers
can select another solution limit through the existing execution options. Parallel logs may interleave.

## Confirmation

Compile the complete single-file workspace in Debug and Release on Windows. Logs must show multiple
MSBuild nodes, the selected project limit, and bounded compiler `/MP` counts. Compare clean rebuilds with
the same sources, toolset and configuration. Every original single-file and standalone target must remain.
