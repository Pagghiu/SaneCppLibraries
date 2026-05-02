# Sane Http Client

## Quick Use

Choose `http-client` when the task needs the separate native-backend client and the user wants to stream bodies or work with the poll-driven core.

- Use `HttpClient` when you want the core poll-driven API.
- Use `HttpClientAsyncT` when you want the async-stream adapter on top of `Async` and `AsyncStreams`.
- Use `HttpClientOperationMemory` and `HttpClientAsyncOperationMemoryT` for caller-owned buffers and queues.
- Request configuration is grouped into headers, body, and `HttpClientRequestOptions`; streamed uploads live in `HttpClientRequestBody`.
- Use the blocking helper only for simple synchronous workflows.

## When Not To Use

- Use `http` if the user needs HTTP server or parser features.
- Expect one in-flight request per operation, with multiple operations sharing one client.
- Redirect policy and TLS hooks exist, but some backend-specific customizations still fail fast.

## References

- [HTTP client streaming](references/http-client-streaming.md)
- Public docs: `Documentation/Libraries/HttpClient.md`
- Tests: `Tests/Libraries/HttpClient/HttpClientTest.cpp`
