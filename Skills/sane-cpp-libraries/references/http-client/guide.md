# Sane Http Client

## Quick Use

Choose `http-client` when the task needs the separate native-backend client and the user wants to stream bodies or work with the poll-driven core.

- Use `HttpClient` when you want the core poll-driven API.
- Use `HttpClientAsyncT` when you want the async-stream adapter on top of `Async` and `AsyncStreams`.
- Use `HttpClientOperationMemory` and `HttpClientAsyncOperationMemoryT` for caller-owned buffers and queues.
- Use the blocking helper only for simple synchronous workflows.

## When Not To Use

- Use `http` if the user needs HTTP server or parser features.
- Expect one in-flight request per client and mostly experimental behavior.

## References

- [HTTP client streaming](references/http-client-streaming.md)
- Public docs: `Documentation/Libraries/HttpClient.md`
- Tests: `Tests/Libraries/HttpClient/HttpClientTest.cpp`
