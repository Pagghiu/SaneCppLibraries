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

Set-Cookie and Cache-Control builders and parsers use the same category while distinguishing empty or missing
cookie fields, conflicting cache visibility, absent directives, and insufficient caller storage. Optional English
formatting remains outside mandatory headers.

Incoming request and response headers share storage, size-limit, and token-limit errors. Their identities no longer
depend on caller-provided literal strings; the same parsing failure propagates through server and client paths.

The fixed-buffer writer accepts an Http error code for its caller-owned output capacity, allowing header emission
and multipart body staging to remain distinct without carrying diagnostic text. Content-Length formatting has a
separate code; its English text stays in the optional formatter.

The multipart writer distinguishes absent, empty, unsafe, and oversized boundaries; malformed field/file names;
unsafe content types; and part-count limits. Field and file paths share identities for the same portable condition.

Incoming message framing reports Content-Length violations, conflicting or unsupported framing headers, invalid
chunk syntax/terminators, trailers, and unsupported pipelined body bytes with portable codes. The same chunk-trailer
identity propagates through server requests and client responses; no chunk text or OS status is retained in Result.

Outgoing message and response failures now distinguish header lifecycle/order, incompatible framing, missing output
streams, invalid response status/reason text, redirect validation, and fixed chunk-header storage. Shared conditions
such as already-sent headers use one identity across request and response paths.

Client request setup shares the outgoing lifecycle identities and adds distinct codes for repeated request starts,
unsupported response content coding, invalid compressed-body coding, and missing multipart writer/boundary state.

Connection-pool initialization distinguishes active-connection lifecycle, empty header storage, per-resource caller
storage shortages, and invalid read/buffer queue configuration. Capacity checks use division rather than multiplying
untrusted counts, so an overflowing per-client requirement cannot evade validation.

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
