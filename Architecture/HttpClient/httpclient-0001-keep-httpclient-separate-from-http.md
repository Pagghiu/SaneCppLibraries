# HTTPCLIENT-0001 - Keep HttpClient separate from Http

Status: Accepted
Date: 2026-07-04

## Context

`Http` already contains an async HTTP/1.1 parser, server, and client built on Sane async streams. `HttpClient` serves a different use case: a standalone HTTP(S) client backed by native operating-system facilities such as `NSURLSession`, WinHTTP, and libcurl, with no dependency on `Http`, `Async`, or `AsyncStreams`.

## Decision

`HttpClient` remains a separate library from `Http`. It must not depend on `Http` for parsing, transport, or convenience helpers, and `Http` must not become a required layer for native-backend HTTP(S) requests. Shared project primitives should come from Common fragments or equivalent dependency-safe sources rather than by coupling the two libraries.

## Consequences

Some HTTP concepts are represented separately in both libraries. In exchange, users can adopt a native-backend client without pulling in the async HTTP stack, and the `Http` async stack can continue to evolve around Sane async streams and server behavior.

## Confirmation

A change preserves this decision when dependency reports and single-file outputs show no `HttpClient` dependency on `Http`, no `Http` dependency on `HttpClient`, and shared behavior is not moved between the two libraries without a new ADR.

## Related

- [SC-0003 - Keep Libraries Independently Consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0008 - Prefer Native OS APIs Over Third-Party Dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [HttpClient documentation](../../Documentation/Libraries/HttpClient.md)
- [Http documentation](../../Documentation/Libraries/Http.md)
