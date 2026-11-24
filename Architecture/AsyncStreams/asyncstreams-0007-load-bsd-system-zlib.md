# ASYNCSTREAMS-0007 - Load BSD system zlib at runtime

Status: Accepted
Date: 2026-11-02

## Context

Zlib compression is an optional runtime facility. The tested BSD base systems provide different zlib
sonames, so using the Linux soname on both can report an unavailable runtime even when system zlib is present.

## Decision

On NetBSD, try `libz.so.1`; on FreeBSD, try `libz.so.6`; on OpenBSD, use `libz.so`. If the NetBSD or FreeBSD default
soname cannot be opened, try `libz.so`. An explicit library path is used as supplied and does not fall back to another
library. Keep symbol resolution and failure reporting within the existing optional dynamic loader.

## Consequences

The tested BSD base systems can use the existing compression transforms without a zlib link dependency. Systems without
either runtime name still receive `CompressionRuntimeUnavailable`. The unversioned fallback permits other compatible
BSD zlib releases when that system supplies the link; it does not claim support for untested BSD variants.

## Confirmation

Exercise the existing gzip, deflate and zlib tests on NetBSD, FreeBSD, and OpenBSD, verify the fallback and explicit-path
failure behavior, and retain the existing-platform and generated single-file checks.
