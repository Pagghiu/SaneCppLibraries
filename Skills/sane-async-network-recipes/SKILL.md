---
name: sane-async-network-recipes
description: Async networking composition recipes for third-party AI agents. Use when deciding how to combine Sane Socket, Async, Async Streams, Http, or Http Client for servers, clients, pipelines, file streaming, timers, wake-up integration, or hot-reload style workflows.
---

# Sane Async Network Recipes

## Quick Use

Choose `sane-async-network-recipes` when the task is architectural rather than library-specific and the user needs the right Sane composition.

- Route raw synchronous sockets and DNS to `sane-socket`.
- Route event-loop-driven I/O and wake-up integration to `sane-async`.
- Route chunked producer/consumer flows to `sane-async-streams`.
- Route async HTTP servers or file servers to `sane-http`.
- Route the separate native HTTP client to `sane-http-client`.

## Recipe Mindset

- Start from the transport the user already has.
- Add `Async` only when blocking I/O is no longer acceptable.
- Add `AsyncStreams` only when the data needs streaming, buffering, or transform stages.
- Add `Http` when the user wants server or parser semantics.
- Add `HttpClient` when the user wants the native backend client or the separate async adapter.

## References

- [composition recipes](references/composition-recipes.md)
- Public docs: `Documentation/Pages/Examples.md`
- Dependency map: `Support/Dependencies/Dependencies.json`
