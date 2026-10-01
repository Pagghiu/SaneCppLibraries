# TOOLS-0002 - Build Package-Local Fil-C Runtime Libraries

Status: Accepted
Date: 2026-10-03

## Context

Fil-C programs cannot load ordinary host C libraries because their pointer ABI differs. Native ARM execution removes translation but does not remove this ABI boundary. Optional runtime libraries must remain optional to library consumers.

## Decision

Build `curl-filc` from pinned, hash-checked upstream sources with the selected native Fil-C compiler. Keep its shared library, headers, receipts and build state under the package directories. Use upstream configure/make rather than maintaining curl's platform source selection in SC-build. Add the package library directory to the Fil-C runner's existing runtime search path alongside `zlib-filc`; do not replace system libraries.

The initial curl profile provides HTTP, threaded DNS and proxies, without TLS, HTTP/2, WebSockets or automatic content decoding. Report loaded-library features accurately and reject unsupported policies. Add compatible TLS/HTTP2 dependencies only after their runtime checks pass.

## Consequences

Consumers retain runtime loading without link dependencies. Package installation needs host make and configure prerequisites. The HTTP-only profile is useful for local integration but is not an HTTPS solution. Fil-C toolchain changes invalidate the package build identity. The pinned curl libtool export-regex workaround must preserve version/SONAME metadata.

## Confirmation

Verify the receipt, native target, loader symbols, HTTP transfer and callbacks. Run actual HttpClient sections in Debug and Release and check unsupported-policy errors. Build artifacts and operational plans remain ignored.
