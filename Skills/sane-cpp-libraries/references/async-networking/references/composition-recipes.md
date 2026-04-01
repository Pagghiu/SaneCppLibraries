# Composition Recipes

## Choose The Smallest Useful Stack

| Task | First Choice | Add Next |
|---|---|---|
| Blocking TCP or UDP client/server | `socket` | `async` only if the task must stop blocking |
| Event-loop driven networking | `async` | `async-streams` if payloads need streaming |
| Async HTTP server or static file server | `http` | `async-streams` for body pipelines |
| Streaming HTTP client with native backend | `http-client` | `async-streams` via `HttpClientAsyncT` |
| Hot-reload or dynamic plugin orchestration | `plugin` plus `async` | `async-streams` for body flow |

## Good Recipes

- Use `SocketDNS` plus `SocketClient` for a quick sync client.
- Use `AsyncEventLoop` plus socket requests for non-blocking I/O.
- Use `AsyncReadableStream` plus `AsyncTransformStream` for a streaming filter or codec.
- Use `HttpAsyncFileServer` for static assets with fixed memory.
- Use `HttpClientAsyncT` when the HTTP client must join a stream pipeline.

## Routing Rules

- If the user says "HTTP server", start with `http`.
- If the user says "HTTP client" and mentions native backends or streaming, start with `http-client`.
- If the user says "event loop" or "wakeup", start with `async`.
- If the user says "pipeline" or "backpressure", start with `async-streams`.

## Best Sources

- `Documentation/Pages/Examples.md`
- `Documentation/Pages/BuildingUser.md`
- `Support/Dependencies/Dependencies.json`
- `Examples/SCExample/HotReloadSystem.h`
