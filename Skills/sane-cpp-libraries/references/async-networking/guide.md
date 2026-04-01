# Sane Async Network Recipes

## Quick Use

Choose `async-networking` when the task is architectural rather than library-specific and the user needs the right Sane composition.

- Route raw synchronous sockets and DNS to `socket`.
- Route event-loop-driven I/O and wake-up integration to `async`.
- Route chunked producer/consumer flows to `async-streams`.
- Route async HTTP servers or file servers to `http`.
- Route the separate native HTTP client to `http-client`.

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
