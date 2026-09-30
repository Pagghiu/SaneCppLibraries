# ASYNC-0007 - Use Portable Structured Result Errors

Status: Accepted
Date: 2026-09-22

## Context

Async reports validation, request-lifecycle, backend, and completion failures through many literal messages. A result
can be stored in a request or delivered to a callback after the initiating call returns. These public paths currently
carry plain `Result`, and `AsyncResult` already names the callback view, so replacing them with another enriched result
would enlarge hot request storage and alter callback APIs.

## Decision

Async owns category 13 and one append-only `AsyncError` enum. Primary values describe portable caller-visible
conditions or operation stages, not native APIs. A backend-specific API name is insufficient reason to add a
platform-specific primary code. Where a portable stage loses useful native diagnostics, a future bounded enriched
result or explicit native-detail API needs its own design and ABI review; this migration does not silently enlarge
request records or change callback signatures.

Async-owned validation and lifecycle errors use its category. Results produced by File, Socket, Threading, caller work,
or any other source preserve their original category and value when stored or propagated through Async. Plain `Result`
contains no owned or borrowed text and no native handle. The normal `Async.h` exposes numeric identity without English
messages; optional `AsyncErrorFormatter.h` supplies canonical English text on demand.

Port producers in cohesive slices: lifecycle and validation, common request setup, then platform backends. Each slice
needs focused identity tests. Behavior changes, especially around cancellation, sequencing, and completion, need
regressions that reproduce the old behavior; changing a literal to an identity alone must not alter the state machine.

Known kernel-completion failures use the request's portable operation identity on io_uring, epoll, kqueue, and Windows
overlapped I/O. Generic completion failure is reserved for unclassified infrastructure operations. This does not remap
foreign File/Socket results from shared operations, and it does not change cancellation or transient-interruption
handling. Refused-connect and invalid-file regressions exercise actual backend failures rather than fabricated codes.

## Consequences

Existing `SC_TRY` and callback paths remain source-compatible. Structured Async failures contain no text, so diagnostic
consumers use the optional formatter or the generic numeric fallback. The primary
code can identify the failed portable stage but cannot recover native error numbers from a plain result.

## Confirmation

Tests assert representative lifecycle producer identities, formatter status and capacity, and foreign result
preservation. The registry validator and all single-file targets must pass. Complete Debug and Release suites must
pass on macOS, Linux, and Windows before the Async port is considered finished.

## Related

- [COMMON-0009 - Use Library-Owned Structured Result Errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [Result error category registry](../Common/result-error-categories.md)
- [Async architecture](async-architecture.md)
