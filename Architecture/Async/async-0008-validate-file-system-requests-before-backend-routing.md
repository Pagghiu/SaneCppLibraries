# ASYNC-0008 - Validate File-System Requests Before Backend Routing

Status: Accepted
Date: 2026-09-25

## Context

`AsyncFileSystemOperation::validate()` ran when a request was submitted through the native event-loop path, but most
methods routed to `AsyncLoopWork` without invoking it first. An invalid path or handle could therefore fail
synchronously on one backend while being accepted for later callback failure on another, or could be obscured by a
missing-thread-pool error. That makes a portable primary `Result` identity unreliable for the same caller mistake.

## Decision

Each public file-system operation validates its constructed request data before selecting the native or thread-pool
path. A validation failure returns synchronously, queues no request, and does not invoke a completion callback. The
existing per-operation check order remains intact. Native submission may validate again as a defensive check; this
does not change success behavior. Backend execution failures retain their originating lower-level identity and are
not translated into an Async validation code.

## Consequences

Some invalid thread-pool requests now fail at the initiating call rather than being accepted and failing through a
callback, and an invalid request is reported before a missing thread pool. Valid requests retain their prior routing
and callback lifecycle. The `AsyncFileSystemOperation` union still owns its request data until destruction or reuse;
the change introduces no allocation or borrowed error text.

## Confirmation

A regression first demonstrated that an empty-source copy failed to return a source-path error before thread-pool
routing. Contract tests check invalid operations, paths, and handles synchronously with no queued requests on macOS,
Linux io_uring and epoll, and Windows. Existing Async file-system integration tests cover valid native and thread-pool
operations. The single-file build and category registry validator must pass.

## Related

- [ASYNC-0007 - Use Portable Structured Result Errors](async-0007-use-portable-structured-result-errors.md)
- [Async architecture](async-architecture.md)
