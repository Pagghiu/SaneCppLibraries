# HTTP Server And Parser Workflows

## Use This When

- Parse HTTP requests or responses.
- Build an async HTTP server or file server.
- Stream HTTP bodies on top of `Async` and `AsyncStreams`.

## Main Types

- `SC::HttpAsyncServer`
- `SC::HttpAsyncFileServer`
- `SC::HttpAsyncClient`
- `SC::HttpConnectionBase`
- `SC::HttpIncomingMessage`
- `SC::HttpMultipartWriter`

## Typical Flows

### Server

1. Create connection storage with fixed buffers.
2. Configure `HttpAsyncServer` or `HttpAsyncFileServer`.
3. Attach request and response streams.
4. Consume bodies incrementally and return fixed-memory responses.

### Parser-Only Work

1. Use the HTTP parser and incoming-message helpers.
2. Read header fields from fixed storage.
3. Inspect body framing before consuming the body stream.

## Notes

- Prefer `Content-Length` and fixed buffers.
- Treat chunked support as limited and keep expectations modest.
- Use `AsyncStreams` when the payload should flow incrementally.

## Best Sources

- `Documentation/Libraries/Http.md`
- `Tests/Libraries/Http/HttpAsyncServerTest.cpp`
- `Tests/Libraries/Http/HttpAsyncFileServerTest.cpp`
- `Tests/Libraries/Http/HttpAsyncClientTest.cpp`
- `Examples/AsyncWebServer/AsyncWebServer.cpp`
