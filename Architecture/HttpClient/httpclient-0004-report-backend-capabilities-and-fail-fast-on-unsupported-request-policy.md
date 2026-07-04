# HTTPCLIENT-0004 - Report backend capabilities and fail fast on unsupported request policy

Status: Accepted
Date: 2026-07-04

## Context

`HttpClient` uses different native backends on Apple, Windows, and Linux. Those backends do not expose identical controls for redirects, HTTP protocol preferences, TLS customization, proxy policy, or content-coding behavior. Silently downgrading unsupported policy would make caller security and protocol assumptions unreliable.

## Decision

`HttpClient` reports compiled backend capabilities through `HttpClientCapabilities` and fails fast when callers require unsupported backend features or request options. Capability checks are available before backend initialization, before request start, and through static names suitable for diagnostics.

## Consequences

Some valid-looking requests fail before transport setup on platforms that cannot honor the requested policy. In exchange, callers can explicitly require behavior they depend on, tests can pin backend differences, and unsupported options are not silently ignored.

## Confirmation

A change preserves this decision when new backend-dependent features are represented in `HttpClientCapabilities`, request start preflights policy against those capabilities, unsupported options return `Result` failures before relying on backend defaults, and capability diagnostics remain allocation-free.

## Related

- [HttpClient capabilities](../../Libraries/HttpClient/HttpClient.h)
- [HttpClient documentation: capability reporting](../../Documentation/Libraries/HttpClient.md#capability-reporting)
- [SC-0008 - Prefer Native OS APIs Over Third-Party Dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [SC-0006 - Use Explicit Result-Based Error Propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
