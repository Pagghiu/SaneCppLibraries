# HTTP-0003 - Keep HTTP parser and body framing incremental, not AST-based

Status: Accepted
Date: 2026-07-04

## Context

HTTP input can arrive in partial buffers, body bytes can follow headers in the same read, and large request or response bodies must stream through caller-owned buffers. Building a complete HTTP message tree would require more storage policy, make backpressure harder, and work against the library's fixed-memory async design.

## Decision

`Http` keeps the HTTP parser incremental and token-oriented. Header parsing reports tokens and preserves views into fixed header storage. Body framing is handled by message state and streams, including content-length, chunked, and close-delimited paths, without materializing a full request or response AST.

## Consequences

The implementation carries explicit parser and body-framing state machines, and callers consume bodies through stream events rather than a collected message object. In exchange, parsing works with partial input, body data can be forwarded incrementally, and large bodies do not require hidden allocation.

## Confirmation

A change preserves this decision when `HttpParser` remains incremental, incoming message bodies can be consumed through `AsyncReadableStream`, chunked and fixed-length body paths do not require full buffering, and APIs that decode or copy data require caller-provided storage.

## Related

- [Http parser](../../Libraries/Http/HttpParser.h)
- [Http incoming message body framing](../../Libraries/Http/HttpConnection.h)
- [Http documentation: description](../../Documentation/Libraries/Http.md)
- [SC-0001 - Library Code Must Not Hide Dynamic Allocation](../Global/sc-0001-no-hidden-allocation.md)
