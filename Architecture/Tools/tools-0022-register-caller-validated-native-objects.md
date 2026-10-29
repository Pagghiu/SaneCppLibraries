# TOOLS-0022 - Register Caller-Validated Native Objects

Status: Accepted
Date: 2026-10-29

## Context

Same-run CI reuse requires installing compatible objects while letting SC-build
own its command and dependency metadata. Workflow scripts must not write private
acceptance markers directly.

## Decision

Provide an explicit ImportObjects action with a caller-owned typed inventory.
Each item identifies the exact source, staged object, compiler dependency file,
consumer working directory and rendered consumer compilation command. Support
one native standalone project with compiler dependencies and a non-MSVC driver
initially. Validate all sources, duplicates, commands, input files and parsed
dependencies before replacing outputs. The caller validates object contents and
compiler/SDK/source/header/ABI compatibility against an immutable source snapshot;
matching a command alone does not establish object compatibility.

SC-build copies accepted objects and dependencies and writes its canonical command
metadata. Remove the previous command marker before copying; a failed import must
not leave that marker certifying incomplete outputs. Set the accepted object time
to registration time because copying may preserve producer timestamps. Imports do
not compile, link or execute. An ordinary compile afterward performs dependency
invalidation, export scanning, linking and stripping normally.

## Confirmation

Prove mismatch rejection before writes, duplicate/unknown source rejection,
missing/malformed dependency rejection, copy failure handling and unsupported
mode rejection. Prove ordinary compilation skips an imported object, links it
correctly and rebuilds after header and flag changes. The CI transport verifier
must independently reject stale revisions, attempts, toolchains, source/header
changes and corrupt payloads before invoking this action. CLI transport and CI
adoption remain separate implementation and validation steps.

## CLI inventory

`build import-objects <project> --manifest PATH` reads UTF-8 text starting with
`SC-BUILD-OBJECTS-1` and LF. Each record has five LF-terminated nonempty lines:
absolute source path, staged object path, staged dependency path, consumer working
directory, exact consumer command from its compilation database. CR, NUL and LF
inside fields are unsupported. Empty or truncated inventories fail. Parsing owns
its backing text until the action completes. This explicit versioned interface
does not expose the backend's private command/dependency metadata layout.

The Fil-C packet verifier stages checked payloads in a new directory, rechecks
current dependency hashes and the consumer compilation database, then emits this
inventory. It cannot directly modify consumer outputs.
