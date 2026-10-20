# ASYNCEXECUTION-0020 - Caller-Owned Payloads

Status: Accepted
Date: 2026-10-20

## Decision

Remove AsyncExecutionBuffer and the buffer-specific onPool overload. Payload memory belongs to the application. Runtime readFile requires Span<char>; writeFile and sendTCP require Span<const char>. Paths remain copied inline, file/socket handles retain move-only ownership, and generic onPool, tryThen and checked value carriers remain. No context-wide allocator, compatibility heap payload, hidden growth or large inline payload is introduced. This supersedes the owned-payload allowance in ADR 0001.

readFile reads an ordinary file through observed EOF, repeating positive short reads. A one-byte probe distinguishes exact capacity from FileTooLarge, including empty destinations. Success emits the exact borrowed prefix. Errors/stop can modify destination bytes; output-object preservation is not payload rollback. Concurrent modifications are not a file snapshot. Reads and complete writes stay on the worker pool and close before terminal delivery; earlier I/O errors take precedence over close errors. Stop joins worker quiescence, without preemption or side-effect rollback.

Example codecs accept disjoint input/output spans and a written-span output reference. Check capacity/arithmetic before writing and preserve the output reference on failure. A thin checked-carrier overload supports tryThen without changing generic pool/result semantics.

Span objects do not own pointees. Lazy/external driving requires persistent backing storage through joined completion. Concurrent writes require disjoint records; application containers must not move/resize active backing storage. run joins before local storage is released. Large records belong in deliberately provisioned application storage.

## Consequences

The adapter performs no payload allocation. Unknown-sized input requires an application capacity policy or existing descriptor/chunk APIs, not a new streaming framework. OS/backend resources and arbitrary provider/user algorithms have separate allocation policies. The experimental API break is intentional.
