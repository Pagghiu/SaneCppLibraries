---
name: sane-http
description: HTTP parser, async server, and HTTP file server guidance for third-party AI agents. Use when working with SC::HttpAsyncServer, SC::HttpAsyncFileServer, SC::HttpAsyncClient, HTTP parsing, body framing, file serving, or when composing HTTP on top of Async and AsyncStreams.
---

# Sane Http

## Quick Use

Choose `sane-http` when the task needs the Sane HTTP parser or the async server/file-server stack.

- Use `HttpAsyncServer` when you need an HTTP server on top of `Async`.
- Use `HttpAsyncFileServer` when you need static file serving.
- Use `HttpAsyncClient` when you want the Sane async client flow.
- Use `HttpMultipartWriter` for multipart form bodies.
- Use `HttpIncomingMessage` and its body stream when the task is about incremental body consumption.

## Use Carefully

- Prefer `sane-http-client` when the user wants the separate native backend HTTP client.
- Expect fixed-memory, content-length-oriented workflows.
- Treat `Http` as the higher-level HTTP composition layer over `Async` and `AsyncStreams`.

## References

- [HTTP server and parser workflows](references/http-server-and-parser-workflows.md)
- Public docs: `Documentation/Libraries/Http.md`
- Tests: `Tests/Libraries/Http/HttpAsyncServerTest.cpp`
- Tests: `Tests/Libraries/Http/HttpAsyncFileServerTest.cpp`
- Examples: `Examples/AsyncWebServer/AsyncWebServer.cpp`
