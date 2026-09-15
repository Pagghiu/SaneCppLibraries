# FILESYSTEMWATCHER-0003 - Use structured watcher errors with backend context

Status: Accepted
Date: 2026-09-14

## Context

FileSystemWatcher failures were identified only by legacy message strings. The watcher has portable lifecycle and
capacity failures, while its Linux, Apple, and Windows backends expose useful implementation stages and native error
values. Async event-loop runners are implemented by another library and must remain a foreign-result boundary.

## Decision

FileSystemWatcher owns category 6 and the portable `FileSystemWatcherError` taxonomy. Backend stages are represented by
the append-only public `FileSystemWatcherErrorDetail` enum, and native error values are copied into
`ResultFileSystemWatcher` as `uint32_t`. Its composed `Result` is authoritative: same-domain copies retain context,
plain and foreign conversions clear inactive context, and conversion to plain `Result` preserves only category/error
identity. The bridge representation is 24 bytes on the supported 64-bit targets and is expected to become 16 bytes
when the legacy `Result` message pointer is removed.

Public watcher operations and private watcher helpers return the enriched result. `EventLoopRunner` virtual methods and
`FileSystemWatcherAsyncT` overrides intentionally continue to return plain `Result`, so failures owned by Async retain
their foreign identity unchanged at that boundary.

Expected completion is not an error. Linux suppression of a coalesced duplicate event returns successful omission, and
Apple's event-refresh result starts successful because it is synchronization state rather than a failure sentinel.

## Consequences

Callers can branch on portable watcher errors and optionally inspect stable backend context without parsing text. Native
error values remain diagnostic payload rather than public primary identity. The enriched type has a temporary ABI cost,
and context is deliberately lost when a result crosses into plain `Result`.

## Confirmation

Tests assert category/error identity, context preservation and clearing, formatting, POD/layout properties,
deterministic validation failures, and structured identity at the Async adapter boundary. The category registry
validator and single-file build must continue to pass.

## Related

- [COMMON-0009 - Use Library-Owned Structured Result Errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [Result error category registry](../Common/result-error-categories.md)
