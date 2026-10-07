# TOOLS-0005 - Reuse Fil-C Zlib With Install-Local Build Identity

## Status

Accepted.

## Context

The rootless Fil-C launcher and its TLS dependency installers call the zlib installer directly.
The installer previously deleted its working directory and rebuilt all zlib sources on every call,
including repeated requests for an already validated package. Download-cache metadata is shared
across install prefixes and cannot by itself identify the compiler that produced a particular library.

## Decision

Reuse pinned-archive zlib artifacts only when the expected libraries and headers exist, the structured
receipt matches the package, exports, source hash and install root, and both shared metadata and an
install-local build identity match. The identity includes source, version, architecture, build profile
and the Fil-C compiler version. Publish the local identity only after successful build, runtime smoke
validation and receipt creation. Bump the build profile when the zlib build recipe changes incompatibly.

Retain the compile-and-run compression/decompression smoke test on every warm hit. Artifact corruption
must fail closed rather than being hidden by a metadata match. Missing artifacts, invalid receipts or
changed identities rebuild through the existing installer. Imported source directories always rebuild:
their paths do not establish immutable content identity.

## Consequences

Warm package requests avoid rebuilding zlib without weakening runtime validation. Existing caches
without the local identity require one rebuild; changed dependency metadata may also invalidate TLS
dependency caches. The compiler pin and third-party build systems are unchanged. Concurrent mutation
of one package prefix is not introduced or supported by this decision.

## Validation

The opt-in SupportToolsTest section `Fil-C zlib warm cache` checks timestamp-preserving reuse,
install-local identity mismatch, missing headers/library, malformed receipt, changed shared identity,
corrupt-library rejection, and modified imported sources. It runs on Linux with
`SC_RUN_HEAVY_SUPPORT_TOOLS_TESTS=1`. Full rootless suites and the actual TLS fixture verify consumers.

See [TOOLS-0002](tools-0002-build-package-local-filc-runtime-libraries.md) and
[TOOLS-0003](tools-0003-package-filc-tls-dependencies.md).
