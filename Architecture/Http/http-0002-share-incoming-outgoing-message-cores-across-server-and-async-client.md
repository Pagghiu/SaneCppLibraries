# HTTP-0002 - Share incoming/outgoing message cores across server and async client

Status: Accepted
Date: 2026-07-04

## Context

Server requests and client responses both parse incoming HTTP messages. Server responses and client requests both build outgoing HTTP messages. Duplicating body framing, header lookup, chunk handling, fixed-header writing, and stream lifecycle logic across those roles would make protocol fixes uneven and tests less representative.

## Decision

`Http` shares incoming message behavior in `HttpIncomingMessage` and outgoing message behavior in `HttpOutgoingMessage`. `HttpRequest` and `HttpAsyncClientResponse` remain thin role-specific wrappers over the incoming core. `HttpResponse` and `HttpAsyncClientRequest` remain thin role-specific wrappers over the outgoing core. Shared protocol behavior belongs in the message cores; request-only, response-only, server-only, or client-only semantics belong in wrappers or owning transport types.

## Consequences

Protocol changes usually affect both server and async-client behavior through one implementation, improving locality and test coverage. The shared cores must avoid role-specific policy leakage, so some role methods stay in wrappers even when they are small.

## Confirmation

A change preserves this decision when common header/body/framing behavior is implemented in `HttpIncomingMessage` or `HttpOutgoingMessage`, public tests exercise shared behavior through the role wrappers, and new role-specific APIs do not duplicate shared parser, body, or header lifecycle logic.

## Related

- [Http library notes: architecture overview](../../Libraries/Http/AGENTS.md)
- [Http test notes: message symmetry](../../Tests/Libraries/Http/AGENTS.md)
- [HttpConnection public message types](../../Libraries/Http/HttpConnection.h)
