# TOOLS-0021 - Compile Native Objects Without Linking

Status: Accepted
Date: 2026-10-29

## Context

Fil-C core and tools targets compile forty compatible library units twice. A
consumer can overlap its distinct compilation with the existing producer, but
needs an explicit phase that cannot link or execute an incomplete target.

## Decision

Expose `build compile-objects <project>` for one native project without workspace
dependencies. An optional `--exclude-sources PATTERN` uses the existing wildcard
matcher on project-relative source paths. Reject an unmatched pattern and reject
this selection on ordinary compile/run actions. Keep the full compilation database
and resolved source inventory. Selected sources use normal commands, dependency
checks and metadata. Skip the final export scan, resource compilation, link and
strip; never execute a preexisting executable. A subsequent ordinary compile
performs all normal checks and final build phases.

## Consequences

Default compilation is unchanged. This action alone neither imports objects nor
establishes their compatibility. Object transfer and validated registration require
a separate supported interface before CI can eliminate duplicate compilation.
Generated backends and workspace builds are deliberately unsupported initially.

## Confirmation

Exercise sequential and parallel selection, absent final executable, unmatched
patterns, ordinary-action misuse, unsupported backends/workspaces, compile failure
propagation, full compilation database, and normal incremental linking afterward.
Retain native Debug/Release and generated-backend regression coverage.
