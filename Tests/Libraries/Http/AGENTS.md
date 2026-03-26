# HTTP Test Notes

This directory contains both public-client coverage and legacy HTTP test helpers.

## Test split

- Prefer the public `SC::HttpAsyncClient` for new tests.
- For streamed or manual request bodies, prefer the active client API: `client.start(...)` plus `client.onRequest(HttpAsyncClientRequest&)`.
- `HttpTestClient` is the old allocating helper kept only for tests that need artificial body splitting or other legacy behaviors.
- `HttpStringAppend.h` in this directory is test-only infrastructure. Do not move it back into `Libraries/Http`.

## Message symmetry

- Server requests and client responses now share the incoming-message shape. Prefer `getReadableStream()`, `getBodyBytesRemaining()`, header lookup helpers, and `consumeBodyBytes()` instead of reaching into socket streams directly.
- Server responses and client requests now share the outgoing-message shape. Prefer `getWritableStream()`, `sendHeaders()`, and `end()` when exercising live body writes.
- Keep tests aligned with the public wrappers. If behavior is shared between request/response roles, exercise it through `HttpRequest`, `HttpAsyncClientResponse`, `HttpResponse`, or `HttpAsyncClientRequest` rather than by poking at internal transport state.

## Useful commands

```bash
./SC.sh build compile SCTest Debug
./SC.sh build run SCTest Debug -- --test "HttpAsyncClientTest"
./SC.sh build run SCTest Debug -- --test "HttpAsyncServerTest"
./SC.sh build run SCTest Debug -- --test "HttpAsyncFileServerTest"
./SC.sh build run SCTest Debug -- --test "HttpKeepAliveTest"
./SC.sh build run SCTest Release -- --test "HttpAsyncClientTest"
```

For section-level debugging:

```bash
./SC.sh build run SCTest Debug -- --test "HttpAsyncClientTest" --test-section "basic GET"
```

## Benchmark reminder

If a test change requires touching HTTP server hot paths, also run the AsyncWebServer benchmark workflow documented in `Libraries/Http/AGENTS.md`.

## Common pitfalls

- These tests are async. Hangs usually mean a readable or writable stream was not closed, paused, or reactivated correctly.
- Use `--port-offset` when running multiple HTTP tests or worktrees in parallel.
- Tests are built with `--nostdinc`; if you need C headers in tests use C headers like `<stdio.h>`, not C++ wrappers like `<cstdio>`.
