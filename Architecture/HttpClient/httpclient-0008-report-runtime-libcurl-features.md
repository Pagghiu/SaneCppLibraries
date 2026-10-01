# HTTPCLIENT-0008 - Report runtime libcurl features

Status: Accepted
Date: 2026-10-03

## Context

The Linux backend dynamically loads libcurl, so protocol and TLS support depend on the library found at runtime, not
the host or the HttpClient binary. This matters for Fil-C, where the package-local curl profile intentionally omits
TLS and HTTP/2. Advertising those capabilities unconditionally would allow callers to rely on policies the loaded
backend cannot honor. The backend also calls the variadic `curl_easy_setopt` and `curl_easy_getinfo` entry points;
callback pointers and option values must retain their declared argument types at each call.

## Decision

Keep the libcurl dependency optional and dynamically loaded. Declare the two variadic function pointers with their
variadic signatures, pass callbacks as function pointers, and pass `long` options as `long` values.

Probe `curl_version_info(CURLVERSION_FIRST)` before client initialization and derive Linux TLS-control and HTTP/2
capabilities from the loaded library's feature bits. HTTP/2 preferred and required are both unsupported when curl
does not report HTTP/2; TLS customization flags are unsupported when curl does not report SSL. Unsupported request
policies continue to fail preflight with their specific `HttpClientError` values. Other Linux capabilities retain
their existing backend-defined values.

Cache the small feature snapshot for the process lifetime. Use a temporary loader for the first probe and close its
handle after reading the feature bits, rather than retaining a library handle or repeatedly loading it for each
capability query. If loading or probing fails, report the runtime-dependent features as unsupported.

## Consequences

Capability reporting remains available before `HttpClient::init()` and describes the actual runtime libcurl build.
An HTTP-only curl package can be used by HttpClient without being mistaken for an HTTPS or HTTP/2 backend. Existing
TLS/HTTP2-enabled curl builds continue to report those features. The feature snapshot does not add a general HTTPS
URL capability; HTTPS transport still depends on libcurl's SSL support and transport result.

## Confirmation

Compare Linux capability fields with `curl_version_info` from the loaded library in tests. Exercise successful HTTP
transfers with supported options, verify unsupported HTTP/2 and TLS policy errors before transport setup, and validate
the HttpClient suite with both ordinary Linux curl and the package-local Fil-C HTTP-only curl build.

## Related

- [HTTPCLIENT-0004 - Report backend capabilities and fail fast on unsupported request policy](httpclient-0004-report-backend-capabilities-and-fail-fast-on-unsupported-request-policy.md)
- [HttpClient capabilities](../../Libraries/HttpClient/HttpClient.h)
