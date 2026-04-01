# Sane Http

## Quick Use

Choose `http` when the task needs the Sane HTTP parser or the async server/file-server stack.

- Use `HttpAsyncServer` when you need an HTTP server on top of `Async`.
- Use `HttpAsyncFileServer` when you need static file serving.
- Use `HttpAsyncClient` when you want the Sane async client flow.
- Use `HttpMultipartWriter` for multipart form bodies.
- Use `HttpIncomingMessage` and its body stream when the task is about incremental body consumption.

## Use Carefully

- Prefer `http-client` when the user wants the separate native backend HTTP client.
- Expect fixed-memory, content-length-oriented workflows.
- Treat `Http` as the higher-level HTTP composition layer over `Async` and `AsyncStreams`.

## References

- [HTTP server and parser workflows](references/http-server-and-parser-workflows.md)
- Public docs: `Documentation/Libraries/Http.md`
- Tests: `Tests/Libraries/Http/HttpAsyncServerTest.cpp`
- Tests: `Tests/Libraries/Http/HttpAsyncFileServerTest.cpp`
- Examples: `Examples/AsyncWebServer/AsyncWebServer.cpp`
