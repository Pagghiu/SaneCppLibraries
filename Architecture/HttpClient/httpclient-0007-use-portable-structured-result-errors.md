# HTTPCLIENT-0007 - Use Portable Structured Result Errors

Status: Accepted
Date: 2026-09-26

## Context

HttpClient currently reports failures with embedded English literals across request validation, native backends,
optional adapters, sessions, and scheduling. Callers cannot reliably distinguish these failures without matching
text, while unused diagnostics can remain in an executable.

## Decision

HttpClient owns built-in category 18 and an append-only `HttpClientError` enum in its own library. Primary codes
describe portable request, response, capacity, and operation failures, not NSURLSession, libcurl, WinHTTP, or OS API
names. Backend-specific evidence, if needed, belongs in optional extended library diagnostics; introducing a
platform-specific primary code requires a separately justified ADR. Errors received from callbacks and other
libraries retain their original category unless an explicit, documented translation is necessary.

The mandatory header carries numeric identities only. Canonical English text is formatted on demand into
caller-provided storage by the optional `HttpClientErrorFormatter.h`; plain `Result` owns no memory and stores no
borrowed strings. During migration, older producers may still use the temporary Common bridge, but that bridge is
removed only after all libraries and callers have moved to structured errors.

The initial slice assigns distinct portable identities to request header syntax, framing-header conflicts, and
request body shape. Sized and chunked streams share identities when the underlying invalid condition is the same.
Request URL syntax/scheme/host, method, redirect/protocol settings, TLS CA path shape, and redirect replayability
follow the same rule; preflight returns the first invalid condition without embedding its text.

## Consequences

Callers can branch on category and code without linking human-readable text. Translators can supply their own
formatter, and diagnostics remain optional. Backend detail does not become part of the stable primary taxonomy.

## Confirmation

The category registry assigns 18 only to HttpClient. Focused request-validation tests check exact category and
code through `validate()` and `operation.start()`, and exercise the opt-in formatter. The standalone library remains
independent of `Http` and the final migration removes the legacy message pointer from `Result`.

## Related

- [Common Result decision](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [HttpClient architecture](httpclient-architecture.md)
