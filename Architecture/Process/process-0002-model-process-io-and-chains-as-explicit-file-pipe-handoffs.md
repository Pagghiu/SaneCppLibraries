# PROCESS-0002 - Model Process I/O and Chains as Explicit File/Pipe Handoffs

Status: Accepted
Date: 2026-07-04

## Context

Process needs to support shell-like workflows such as inheriting standard streams, suppressing output, redirecting to buffers, feeding stdin, connecting external pipes, and chaining multiple child processes. Hiding that behavior behind command strings or implicit allocation would make descriptor ownership and failure cases hard to review.

## Decision

`Process` models standard stream behavior through explicit `StdIn`, `StdOut`, and `StdErr` wrappers over spans, growable buffers, files, pipes, inherit, or ignore choices. `ProcessChain` connects caller-owned `Process` objects with `PipeDescriptor` handoffs. The dependency on File is intentional because Process owns native descriptor and pipe integration.

## Consequences

Callers can see which descriptors are inherited, moved, ignored, or read back into caller-owned storage. Process chains require stable caller-owned `Process` objects and explicit waiting. The API is more verbose than shell command composition, but descriptor ownership and error reporting remain visible.

## Confirmation

A change preserves this decision when new process I/O paths are represented as explicit stream/file/pipe/span/buffer choices, external descriptors are validated before use, `ProcessChain` does not allocate hidden process nodes, and the File dependency remains the only dependency needed for native descriptor handoff.

## Related

- [Process documentation](../../Documentation/Libraries/Process.md)
- [Process public API](../../Libraries/Process/Process.h)
- [Process implementation](../../Libraries/Process/Process.cpp)
- [Process chain tests](../../Tests/Libraries/Process/ProcessTest.cpp)
- [SC-0006 - Use explicit Result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
