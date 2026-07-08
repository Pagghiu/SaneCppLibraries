# FIBERSASYNC-0003 - Return Result And Operation-Specific Caller-Owned Output Objects

Status: Accepted
Date: 2026-07-07

## Context

Fiber I/O helpers need to report operation errors and operation-specific values such as byte counts, disconnection, read
spans, peer addresses, process exit status, or signal delivery counts. SC convention keeps `Result` for errors only and
uses caller-provided output objects for additional values.

## Decision

`FiberAsyncIO` methods return plain `Result`. Additional operation data is written into explicit caller-provided result
objects or optional pointer outputs. Buffer data is reported as spans into caller-provided buffers, not newly allocated
storage.

## Consequences

The API stays consistent with lower-level SC libraries and avoids inventing `Result<T>` for fiber I/O. Call sites remain
slightly more verbose than tuple-returning APIs, but ownership and allocation remain visible.

## Confirmation

A change preserves this decision when new `FiberAsyncIO` operations return `Result`, use reference outputs for required
additional data, use pointer outputs for optional additional data, and do not allocate returned payload buffers.

## Related

- [SC-0006 - Use explicit result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
- [AWAIT-0005 - Keep cancellation cooperative and Result-based](../Await/await-0005-keep-cancellation-cooperative-and-result-based.md)
- [FibersAsync architecture](fibersasync-architecture.md)
