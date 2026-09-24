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
Proxy configuration distinguishes invalid mode, URL scheme/host/path, unsafe bytes, malformed authorization or
bypass values, and proxy settings supplied without HTTP-proxy mode. These are request-policy errors regardless of
which native backend subsequently carries out the request.
Capability preflight reports the unsupported policy or feature, rather than naming the current OS backend. A
required-backend mismatch and a generic required-feature failure retain portable identities; specific request
options use specific codes so callers can tell which requested policy cannot be honored.
Client and operation initialization now distinguish repeated initialization, an uninitialized client, missing
caller-owned event/header/metadata storage, empty or insufficient response buffers, and operation state. Repeated
cancel or start attempts return portable operation-state codes; no native backend detail is exposed.
The request-body callback path reports a missing provider, empty destination, provider overflow or no-progress,
and declared-size mismatch through `outError`. An empty destination previously returned a value converted from a
failed `Result` while leaving `outError` successful; the callback contract test exercises this internal helper
without relying on a native backend to supply a zero-length buffer. Provider-returned errors remain unchanged.
Response delivery distinguishes insufficient per-buffer capacity, cancellation, an invalid queued buffer index,
missing response state, insufficient effective-URL metadata storage, and insufficient caller output for a blocking
body. The same category/code is returned through ordinary polling or blocking APIs without carrying text in Result.

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
