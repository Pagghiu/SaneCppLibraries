# ASYNC-0004 - Separate File Readiness From External Completion Injection

Status: Accepted
Date: 2026-07-04

## Context

File-descriptor readiness and externally-submitted completions both interact with the event loop, but they are different concepts. Combining them makes it unclear whether a request means "watch this descriptor" or "let outside code complete this operation".

## Decision

`AsyncFileReadiness` models operating-system readiness notifications for file descriptors. `AsyncExternalCompletion` models manual completion posting and externally-submitted completion operations, including Windows IOCP integration through an externally provided overlapped handle.

## Consequences

Callers choose the primitive that matches the operation they are integrating. The API has two request types instead of one more generic type, but each request has clearer validation, cancellation, and completion semantics.

## Confirmation

A change preserves this decision when readiness monitoring and external completion injection remain separate public request types, tests exercise both paths independently, and new integrations do not overload readiness APIs to carry manually posted completions.

## Related

- [AsyncFileReadiness and AsyncExternalCompletion](../../Libraries/Async/Async.h)
- [Async documentation](../../Documentation/Libraries/Async.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
