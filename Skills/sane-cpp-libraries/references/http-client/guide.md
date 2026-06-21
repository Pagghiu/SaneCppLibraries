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
- Request URL, method, redirect mode, headers, and proxy URL shape are validated in `start()` before backend setup; core requests support `http://` and `https://` URLs.
- Explicit HTTP proxy URLs must be caller-owned, host-only `http://` values with no path, query, fragment, whitespace, or control bytes.
- Custom TLS CA paths are caller-owned and must not contain NUL or other control bytes; unsupported CA path policies fail during preflight.
- Protocol, TLS, and proxy policies are preflighted against `HttpClientCapabilities` in `start()`.
- Use `HttpClientSession` only as an optional caller-owned layer for cookies, cached authorization values, Basic auth challenge helpers, and retry bookkeeping.
- Session cookie capture rejects unrelated `Domain` attributes, ignores URL ports for host scope, and applies path-segment matching without allocating.
- Cached session authorization origins must be exact `http://` or `https://` origins with no path, query, or fragment; values are copied into caller-owned scratch and must not contain CR, LF, or NUL bytes.
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
- Treat the blocking helper response body span as a hard capacity; overflow returns an error and reports copied bytes.
- Recommended flow: validate/preflight request shape, start one request per operation, poll until completion, then reuse the operation only after it is no longer in flight.
- For many poll-driven operations, use `HttpClientOperationScheduler` rather than adding batching policy to `HttpClientOperation`.
- Point users to `Examples/SaneHttpGet` for blocking usage, `Examples/HttpClientAsyncGet` for async-stream usage, and `Examples/HttpClientPollSession` for poll-driven session/scheduler usage.

## When Not To Use

- Use `http` if the user needs HTTP server or parser features.
- Expect one in-flight request per operation, with multiple operations sharing one client.
- Redirect policy and TLS hooks exist, but some backend-specific customizations still fail fast.
- Do not expect the core client to own decompression, cookies, credentials, or retry state.

## References

- [HTTP client streaming](references/http-client-streaming.md)
- Public docs: `Documentation/Libraries/HttpClient.md`
- Tests: `Tests/Libraries/HttpClient/HttpClientTest.cpp`
