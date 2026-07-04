# HTTPCLIENT-0006 - Keep content coding out of the core transport operation

Status: Accepted
Date: 2026-07-04

## Context

HTTP content codings such as gzip, deflate, and brotli are useful but require streaming transforms, extra state, and sometimes additional dependencies or allocation policy. Applying decompression transparently inside `HttpClientOperation` would hide memory and dependency choices from callers.

## Decision

`HttpClientOperation` does not request or decode compressed content on behalf of callers. The core client exposes content-coding metadata, helpers for classifying `Content-Encoding` tokens, and caller-buffer helpers for building `Accept-Encoding` values. Decompression belongs in an explicit caller-owned streaming transform layered above the raw response body.

## Consequences

Callers that want compressed responses must opt in by sending appropriate headers and composing a decoder outside the core operation. The transport core stays dependency-free and avoids hidden buffers or decompression state, while still exposing enough metadata for callers to build that layer deliberately.

## Confirmation

A change preserves this decision when `HttpClientCapabilities::contentCodingPolicy` does not imply transparent core decompression, response content-coding helpers remain metadata-oriented, `Accept-Encoding` construction writes into caller-provided buffers, and any decompression integration is an explicit adapter or higher layer.

## Related

- [HttpClient documentation: content-coding policy](../../Documentation/Libraries/HttpClient.md#content-coding-policy)
- [HttpClient content coding helpers](../../Libraries/HttpClient/HttpClient.h)
- [SC-0003 - Keep Libraries Independently Consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0011 - Make Allocation-Capable Facilities Explicit and Optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
