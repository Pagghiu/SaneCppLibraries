# HTTPCLIENT-0005 - Keep session and scheduler as optional caller-owned layers

Status: Accepted
Date: 2026-07-04

## Context

Stateful HTTP workflows need cookies, authorization cache entries, retry bookkeeping, prepared request headers, and efficient polling across many operations. Making that state implicit in the transport core would add hidden ownership and make simple one-operation clients pay for session or scheduling policy they do not need.

## Decision

`HttpClientSession` and `HttpClientOperationScheduler` remain optional caller-owned layers above `HttpClientOperation`. The session layer prepares request metadata and records response metadata using caller-provided spans and scratch buffers. The scheduler tracks ready initialized operations through caller-provided operation pointers and ready bytes; it does not own operations, start requests, or replace the single-operation request/response contract.

## Consequences

Callers that need stateful or high-concurrency behavior must wire the optional helpers explicitly. The core transport stays small and stateless, and advanced policy remains visible in the types and memory descriptors the caller chooses.

## Confirmation

A change preserves this decision when session state is stored in `HttpClientSessionMemory`, scheduler state is stored in `HttpClientOperationSchedulerMemory`, callers still drive `HttpClientOperation::start()` directly, and neither helper allocates hidden durable storage or owns operations.

## Related

- [HttpClient session layer](../../Libraries/HttpClient/HttpClientSession.h)
- [HttpClient operation scheduler](../../Libraries/HttpClient/HttpClientScheduler.h)
- [HttpClient documentation: session layer](../../Documentation/Libraries/HttpClient.md#session-layer)
- [HttpClient documentation: operation scheduler](../../Documentation/Libraries/HttpClient.md#operation-scheduler)
- [SC-0011 - Make Allocation-Capable Facilities Explicit and Optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
