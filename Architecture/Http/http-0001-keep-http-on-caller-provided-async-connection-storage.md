# HTTP-0001 - Keep HTTP on caller-provided async connection storage

Status: Accepted
Date: 2026-07-04

## Context

`Http` hosts an async HTTP/1.1 parser, server, client, and file-server utility on top of async socket and stream libraries. Connections need read queues, write queues, async buffers, header memory, stream memory, socket state, and per-message state without hidden allocation.

## Decision

`Http` keeps async connection storage caller-provided. Server connections use caller-owned `HttpConnection` storage grouped by `HttpConnectionsPool`, and client connections use caller-owned `HttpConnectionBase` storage such as `HttpAsyncClientConnection`. Fixed-size helpers such as `HttpStaticConnection` may wire compile-time storage into these objects, but the library must not allocate connection, header, stream, or queue storage internally.

## Consequences

Callers must size connection pools and buffers explicitly, and storage exhaustion is part of normal `Result`-based failure handling. In exchange, HTTP memory ownership, latency, and single-file behavior remain predictable, and server/client connection capacity is visible at the call site.

## Confirmation

A change preserves this decision when `HttpAsyncServer`, `HttpAsyncClient`, `HttpAsyncFileServer`, `HttpConnection`, and related stream/message paths continue to receive connection and buffer storage from callers or fixed-storage helper types, and out-of-space cases fail explicitly instead of allocating hidden fallback storage.

## Related

- [SC-0001 - Library Code Must Not Hide Dynamic Allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0011 - Make Allocation-Capable Facilities Explicit and Optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
- [Http library notes](../../Libraries/Http/AGENTS.md)
- [Http documentation](../../Documentation/Libraries/Http.md)
