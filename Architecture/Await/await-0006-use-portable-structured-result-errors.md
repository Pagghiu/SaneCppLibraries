# AWAIT-0006 - Use Portable Structured Result Errors

Status: Accepted
Date: 2026-09-25

## Context

Await stores plain `Result` in coroutine promises and awaiters. Cancellation and wrong-event-loop predicates currently
compare stable message pointers, while many other Await-owned failures return string literals. These identities must
survive coroutine propagation without making English text mandatory or enlarging promise state.

## Decision

Await owns category 15 and one append-only `AwaitError` enum for its own lifecycle, allocator, task, awaiter, and
bounded-storage failures. Primary codes describe portable conditions, not platform APIs. `AwaitCancelledResult()` and
`AwaitWrongEventLoopResult()` return codes, and their exported predicates check category and value. `SC_CO_TRY` keeps
forwarding the concrete plain `Result`; errors produced by Async, File, Socket, or caller work retain their category.
The mandatory header contains numeric identity only. Optional `AwaitErrorFormatter.h` supplies canonical English text
into caller-provided storage.

The old message getter functions remain temporarily during the compatibility bridge but are not used for identity.
The bridge-removal audit must remove or isolate them so unused English strings are linker-evictable. A separate typed
coroutine result or native-detail API would require an explicit promise-size and lifetime review.

## Consequences

Cancellation and wrong-loop checks no longer depend on string address identity. Existing promise, awaiter, and task
storage sizes and control flow stay unchanged. Ordinary callers still use `SC_CO_TRY` or `Result` checks.

## Confirmation

Tests assert the exported predicates and identities, allocator/task producer codes, formatter status, and foreign
`SC_CO_TRY` propagation. Full Debug and Release suites on macOS, Linux, and Windows, the category validator, and all
single-file targets must pass before the Await port is considered finished.

## Related

- [COMMON-0009 - Use Library-Owned Structured Result Errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [AWAIT-0005 - Keep Cancellation Cooperative And Result-Based](await-0005-keep-cancellation-cooperative-and-result-based.md)
