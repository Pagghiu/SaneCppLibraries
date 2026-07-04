# HTTP-0004 - Keep transport concerns in HttpAsyncClient, outside message types

Status: Accepted
Date: 2026-07-04

## Context

The async client must parse URLs, open sockets, decide connection reuse, inject transport-derived defaults such as `Host`, dispatch HTTPS setup hooks, and hand upgraded WebSocket transports to another owner. These concerns are not properties of an HTTP request or response message itself.

## Decision

`HttpAsyncClient` owns async-client transport concerns. Message types such as `HttpAsyncClientRequest`, `HttpAsyncClientResponse`, `HttpIncomingMessage`, and `HttpOutgoingMessage` remain HTTP message surfaces. Transport setup hooks, TLS readiness checks, origin reuse, DNS/connect lifecycle, and WebSocket transport detach stay in `HttpAsyncClient` or adjacent transport-owner code.

## Consequences

Message types stay reusable and symmetric with server-side types, while transport behavior has one owner. Transport features may require client-level callbacks or setup objects instead of adding convenience state to request/response messages.

## Confirmation

A change preserves this decision when new async-client transport features are configured on `HttpAsyncClient` or explicit transport setup objects, message types do not start owning sockets, DNS, TLS, or connection reuse policy, and WebSocket upgrades transfer transport ownership through explicit detach/transport-view APIs.

## Related

- [Http library notes: client-specific notes](../../Libraries/Http/AGENTS.md)
- [HttpAsyncClient](../../Libraries/Http/HttpAsyncClient.h)
- [Http WebSocket transport integration](../../Libraries/Http/HttpWebSocket.h)
- [SC-0009 - Isolate Platform-Specific Implementations Behind Internal Code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
