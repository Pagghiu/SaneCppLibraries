# Sane Http Client

## Quick Use

Choose `http-client` when the task needs the separate native-backend client and the user wants to stream bodies or work with the poll-driven core.

- Use `HttpClient` when you want the core poll-driven API.
- Use `HttpClientAsyncT` when you want the async-stream adapter on top of `Async` and `AsyncStreams`.
- Use `HttpClientOperationMemory` and `HttpClientAsyncOperationMemoryT` for caller-owned buffers and queues.
- Keep URL, header, body, provider, and option string storage alive until the operation completes or is cancelled.
- Request configuration is grouped into headers, body, and `HttpClientRequestOptions`; streamed uploads live in `HttpClientRequestBody`.
- Use the request method, body framing, redirect, protocol, and proxy name helpers for allocation-free diagnostics.
- Use `HttpClientRequest::validate()` when callers want to check request shape before starting an operation.
- Request method, redirect mode, and headers are validated in `start()` before backend setup.
- Protocol, TLS, and proxy policies are preflighted against `HttpClientCapabilities` in `start()`.
- Use `HttpClientSession` only as an optional caller-owned layer for cookies, cached authorization values, Basic auth challenge helpers, and retry bookkeeping.
- Use `HttpClientSessionAuthChallenge::getTargetName()` and `getSchemeName()` for no-allocation auth diagnostics.
- Use `HttpClientSession::findCookie()`, `hasCookie()`, `findAuthorization()`, `hasAuthorization()`, count helpers, and targeted clear helpers to inspect or reset optional session state without allocation.
- Use retry state/count helpers and `HttpClientSession::isRetryableStatusCode()` when coordinating caller-owned retry loops.
- Use `HttpClientOperationScheduler` when an application wants to poll ready operations without scanning every operation.
- Use scheduler count/registration helpers for diagnostics; keep batching policy outside the core operation API.
- Use `HttpClientResponse::getProtocolName()` when logging negotiated protocol metadata.
- Use the `HttpClientResponse::is*Status()` helpers for non-allocating status-code classification.
- Use `HttpClientResponse::hasHeader()`, `hasContentCoding()`, and `hasTransferCoding()` for no-allocation presence checks.
- Use `HttpClientResponse::getNextTransferCoding()` when inspecting response transfer framing diagnostics.
- Use `HttpClientContentCoding::writeAcceptEncoding()` and `HttpClientResponse::getNextContentCoding()` around caller-owned decompression transforms.
- Use `HttpClientCapabilities::supports()`, `supportsAll()`, `supportsRequestOptions()`, `requireRequestOptions()`, `requireFeatures()`, `requireBackend()`, `getBackendName()`, and `getFeatureName()` for no-allocation backend diagnostics.
- Use `HttpClient::init(requiredBackend/features)` when startup should fail unless a specific backend capability set is active.
- Use the blocking helper only for simple synchronous workflows.
- Recommended flow: validate/preflight request shape, start one request per operation, poll until completion, then reuse the operation only after it is no longer in flight.
- For many poll-driven operations, use `HttpClientOperationScheduler` rather than adding batching policy to `HttpClientOperation`.

## When Not To Use

- Use `http` if the user needs HTTP server or parser features.
- Expect one in-flight request per operation, with multiple operations sharing one client.
- Redirect policy and TLS hooks exist, but some backend-specific customizations still fail fast.
- Do not expect the core client to own decompression, cookies, credentials, or retry state.

## References

- [HTTP client streaming](references/http-client-streaming.md)
- Public docs: `Documentation/Libraries/HttpClient.md`
- Tests: `Tests/Libraries/HttpClient/HttpClientTest.cpp`
