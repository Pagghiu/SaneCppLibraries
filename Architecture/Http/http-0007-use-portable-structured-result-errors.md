# HTTP-0007 - Use Portable Structured Result Errors

Status: Accepted
Date: 2026-09-25

## Context

Http currently has hundreds of literal error producers across URL parsing, headers, connection and message state,
async server/client transport, files, and WebSocket framing. Exact English-message tests have made error text act as
machine identity. This retains strings in clients and prevents a caller from reliably selecting recovery behavior.

## Decision

Http owns built-in category 17 and an append-only `HttpError` enum in `Libraries/Http/HttpError.h`. Its primary codes
describe portable protocol, parsing, state, capacity, and operation failures. English text lives only in the optional
`HttpErrorFormatter.h`, which formats into caller-provided storage. Plain `Result` remains allocation-free and
non-owning. Errors returned by Async, Socket, File, Cryptography, user callbacks, or another library pass through
unchanged unless Http explicitly translates them for a documented reason.

The first migration slice distinguishes malformed percent escapes, insufficient decode storage, request-target
validation, URL syntax/protocol, path, host, IPv6 host, user information, and port errors. Boolean preflight failures
that previously became uncategorized `Result(false)` receive an Http code. A normal iterator exhaustion or absent query
value remains a non-error `bool` outcome.

The route matcher retains its non-error `Matched`, `MethodNotAllowed`, `NotFound`, and `TooManyParams` statuses;
`formatAllowHeader` reports only insufficient caller storage as an Http error and forwards request-target parse failures
without replacing their identity.

Multipart parsing now reports invalid/oversized boundaries, empty disposition fields, malformed syntax, and internal
span/candidate-bound violations with Http codes. Once its final boundary has been consumed, repeated `parse` calls
return success with `state` and `token` equal to `Finished`, zero bytes consumed, and empty parsed data. Completion is
not a failure; callers can continue to use the existing state/token outputs without a new allocation or result type.

Authorization helpers distinguish missing credentials, wrong Bearer/Basic scheme, malformed Base64 forms, missing
username/password separator, invalid username, empty tokens/credentials, and caller output capacity. Each condition
keeps a portable Http identity; no credentials or other borrowed text are retained in plain `Result`.

One Http category is sufficient for now even though the library has several domains; error values are grouped and
appended within the local enum as each cohesive port lands. Platform-specific failures do not become primary codes.
Backend stage and native error numbers can be carried only by a separately designed bounded Http-specific enriched
result, with an ADR, and are dropped when converted to plain `Result`.

## Consequences

Ordinary callers continue returning or forwarding `Result` through `SC_TRY`. Callers requiring stable identity inspect
category and value, and those requiring canonical English text opt into the formatter. The temporary legacy bridge
remains only until the full repository producer/consumer audit is clear.

## Confirmation

Focused URL tests assert category/value and formatter behavior rather than exact legacy messages. Http client/server
groups, standalone single-file libraries, request-parsing benchmark, and all-platform Debug/Release suites are
promotion gates for this port.

## Related

- [Common structured Result decision](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [Http architecture](http-architecture.md)
