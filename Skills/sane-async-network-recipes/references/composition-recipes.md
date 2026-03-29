# Composition Recipes

## Choose The Smallest Useful Stack

| Task | First Choice | Add Next |
|---|---|---|
| Blocking TCP or UDP client/server | `sane-socket` | `sane-async` only if the task must stop blocking |
| Event-loop driven networking | `sane-async` | `sane-async-streams` if payloads need streaming |
| Async HTTP server or static file server | `sane-http` | `sane-async-streams` for body pipelines |
| Streaming HTTP client with native backend | `sane-http-client` | `sane-async-streams` via `HttpClientAsyncT` |
| Hot-reload or dynamic plugin orchestration | `sane-plugin` plus `sane-async` | `sane-async-streams` for body flow |

## Good Recipes

- Use `SocketDNS` plus `SocketClient` for a quick sync client.
- Use `AsyncEventLoop` plus socket requests for non-blocking I/O.
- Use `AsyncReadableStream` plus `AsyncTransformStream` for a streaming filter or codec.
- Use `HttpAsyncFileServer` for static assets with fixed memory.
- Use `HttpClientAsyncT` when the HTTP client must join a stream pipeline.

## Routing Rules

- If the user says "HTTP server", start with `sane-http`.
- If the user says "HTTP client" and mentions native backends or streaming, start with `sane-http-client`.
- If the user says "event loop" or "wakeup", start with `sane-async`.
- If the user says "pipeline" or "backpressure", start with `sane-async-streams`.

## Best Sources

- `Documentation/Pages/Examples.md`
- `Documentation/Pages/BuildingUser.md`
- `Support/Dependencies/Dependencies.json`
- `Examples/SCExample/HotReloadSystem.h`
