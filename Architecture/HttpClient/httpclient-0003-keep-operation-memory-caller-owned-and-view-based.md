# HTTPCLIENT-0003 - Keep operation memory caller-owned and view-based

Status: Accepted
Date: 2026-07-04

## Context

Native HTTP backends need request metadata, header buffers, response body buffers, event queues, transport metadata, backend scratch space, and streamed-body callbacks. Hiding ownership behind heap strings or containers would make memory failure behavior less visible and weaken the library's standalone single-file story.

## Decision

`HttpClient` operation memory remains caller-owned and view-based. `HttpClientRequest` may be copied by value at start time, but referenced URLs, headers, body bytes, providers, and option string views remain caller-owned. `HttpClientResponse` stores views into caller-provided response header and metadata buffers. `HttpClientOperationMemory` supplies response buffers, event slots, response headers, response metadata, and backend scratch storage.

## Consequences

Callers must keep referenced request data and operation memory alive for the request lifetime. The library can fail early on missing or undersized storage and does not need to introduce owned strings, heap containers, or hidden response buffering.

## Confirmation

A change preserves this decision when operation initialization validates caller-provided memory, request and response APIs document view lifetimes, response body data is delivered through caller-owned buffers or callbacks, and new durable operation state is not allocated internally.

## Related

- [SC-0001 - Library Code Must Not Hide Dynamic Allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0012 - Support Bring-Your-Own Containers](../Global/sc-0012-support-bring-your-own-containers.md)
- [HttpClient operation memory](../../Libraries/HttpClient/HttpClient.h)
- [HttpClient allocation-free audit](../../Documentation/Libraries/HttpClient.md#allocation-free-audit)
