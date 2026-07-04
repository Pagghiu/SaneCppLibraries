# HTTPCLIENT-0002 - Make the core API poll-driven, with AsyncStreams as an adapter

Status: Accepted
Date: 2026-07-04

## Context

`HttpClient` needs to expose native asynchronous backends without depending on Sane `Async`, `AsyncStreams`, `Threading`, or `Time`. At the same time, Sane applications should be able to compose HTTP response and request bodies with async stream pipelines when they opt into those libraries.

## Decision

`HttpClientOperation` remains the core request/response interface and is driven by `poll()`. Async stream integration is provided by the optional `HttpClientAsyncT<T_AsyncEventLoop, T_AsyncStreams>` adapter, which translates the same operation model into readable and writable streams without changing the core operation contract.

## Consequences

Core users must drive polling or use an adapter, and high-level event-loop integration is deliberately outside the base operation. In exchange, `HttpClient` stays dependency-free, while AsyncStreams users get a composable stream adapter over the same caller-owned operation memory model.

## Confirmation

A change preserves this decision when `HttpClientOperation` remains usable without including `Async` or `AsyncStreams`, async stream support stays in `HttpClientAsyncT` or similar adapters, and adapters drive the public operation interface instead of bypassing it with a second client core.

## Related

- [HttpClient documentation: poll-driven and async patterns](../../Documentation/Libraries/HttpClient.md)
- [HttpClient operation](../../Libraries/HttpClient/HttpClient.h)
- [HttpClient async adapter](../../Libraries/HttpClient/HttpClientAsync.h)
- [SC-0003 - Keep Libraries Independently Consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
