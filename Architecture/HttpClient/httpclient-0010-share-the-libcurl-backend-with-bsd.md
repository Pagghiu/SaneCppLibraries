# HTTPCLIENT-0010 - Share the libcurl backend with BSD

Status: Accepted
Date: 2026-11-02

## Context

NetBSD, FreeBSD, and OpenBSD provide POSIX threads and can use libcurl, but a libcurl runtime is not guaranteed
to be installed. Its available TLS and HTTP/2 features depend on the particular runtime build.

## Decision

Use the existing dynamically loaded libcurl backend on NetBSD, FreeBSD, and OpenBSD as well as Linux. Keep
runtime loading optional and use the system dynamic loader's search for `libcurl.so.4`, followed by
`libcurl.so`. Preserve the compiled-backend and cached runtime-feature semantics of HTTPCLIENT-0008.
If loading the runtime or its required symbols fails, initialization returns
`HttpClientError::RequiredBackendUnavailable`.

## Consequences

The same request, event, synchronization, and policy implementation serves all four platforms.
BSD deployments must make a compatible libcurl runtime visible to the dynamic loader. BSD support
remains experimental and does not imply that the rest of the library suite is fully ported.

## Confirmation

Run the shared HttpClient tests against real NetBSD, FreeBSD, and OpenBSD runtimes, check runtime feature
reporting with their libcurl builds, and verify that a missing runtime fails initialization explicitly.

## Related

- [HTTPCLIENT-0008 - Report runtime libcurl features](httpclient-0008-report-runtime-libcurl-features.md)
- [HttpClient documentation](../../Documentation/Libraries/HttpClient.md)
