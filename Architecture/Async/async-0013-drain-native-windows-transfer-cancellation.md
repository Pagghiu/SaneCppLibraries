# ASYNC-0013 - Drain Native Windows Transfer Cancellation

Status: Accepted
Date: 2026-10-18

## Context

Native FileRead/FileWrite/FileSend and SocketSend inherited a no-op cancellation hook on Windows. A direct kernel regression confirmed that a pending pipe read remained attached to its handle after the stop callback. Caller-owned buffers and OVERLAPPED storage could therefore be reused before kernel quiescence.

## Decision

Give these native overlapped transfers explicit cancellation hooks. Request CancelIoEx on the actual file or socket handle and retain Flag_WaitingKernelCancel until the request's IOCP packet is drained. Wait for that packet when cancellation succeeds and when ERROR_NOT_FOUND means completion may already have won. SC does not suppress synchronous-success IOCP notifications on associated handles, so an active submission still owns one packet.

Track a completion consumed after logical stop with one internal flag, cleared by normal request reuse. Dispatch can consume the packet before the deferred cancellation API phase runs; that phase must then skip cancellation/waiting rather than await a second packet.

Use the existing cancellation queue and event validation path; deliver the stop callback only after the waiting flag is cleared. Draining preserves the existing cancelled-request protocol: it suppresses normal completion delivery rather than surfacing unobserved native errors. Adapters still preserve errors received through normal completion. Worker and manual-completion paths keep their existing joining rules and bypass native cancellation. Preserve the requirement that callers keep descriptors valid through quiescence; unexpected cancellation API failures report CancellationFailed.

## Consequences

All control-flow adapters inherit the same buffer/operation-storage lifetime correction. Cancellation can require another poll to drain either an aborted or successful completion. This adds no public fields, allocations or dependency. Other operation-specific cancellation policies remain separate.

## Confirmation

After cancelling a native pending pipe read, CancelIoEx(handle, nullptr) must report ERROR_NOT_FOUND. Check zero normal callbacks and exactly one stop callback, safe buffer reuse, completion-winning cancellation races, and native file/socket cancellation regressions in Windows Debug/Release. Retain cross-platform default/sender and amalgamation gates.

## Related

- [Microsoft CancelIoEx lifetime and completion rules](https://learn.microsoft.com/windows/win32/api/ioapiset/nf-ioapiset-cancelioex)
