# CRYPTOGRAPHY-0009 - Use structured Cryptography result errors

Status: Accepted
Date: 2026-09-20

## Context

Cryptography previously represented validation, lifecycle, authentication, capacity, native-provider, and runtime
provider failures with string literals. Equivalent failures used unrelated text across CommonCrypto, Windows CNG,
Linux AF_ALG, and OpenSSL, and callers could not inspect the failure without retaining English strings.

## Decision

Cryptography owns category 9 and one portable `CryptographyError` taxonomy. Primary errors describe caller-visible
conditions such as invalid sizes, overlap, uninitialized sessions, authentication failure, capacity, and portable
backend stages. Operating-system and provider names are forbidden in primary errors.

`CryptographyErrorDetail` identifies the logical operation or backend stage. Backend details are explicitly qualified
with Apple CommonCrypto, Windows BCrypt, Linux AF_ALG or `getrandom`, and OpenSSL 3 names. Authentication failures from
all providers share one portable primary identity; provider-specific detection remains a detail.

`ResultCryptography` composes the authoritative `Result` with fixed-width detail and context tags and one 32-bit scalar
payload. Context distinguishes CommonCrypto status, NTSTATUS, POSIX errno, expected bytes, required bytes, maximum bytes,
and actual bytes. A size is attached only when exactly representable; values are never truncated. The result owns no
memory and retains no borrowed strings.

OpenSSL numeric error codes are initially omitted. Correct attribution requires an empty-on-entry queue policy and
`ERR_peek_last_error`; the existing scope deliberately preserves caller errors. POSIX dynamic-loader failures also do
not acquire errno context because `dlopen` and `dlsym` do not define errno as their diagnostic channel.

Feature queries continue to report unavailable providers and algorithms through `Features` while returning success.
The migration does not begin checking cleanup calls or any previously ignored backend call. Plain-result conversion
preserves category/error identity and deliberately loses Cryptography detail and context.

The exported APIs return `ResultCryptography` so domain-aware callers can retain diagnostics. This changes binary ABI
during the unintegrated result branch; clients must be rebuilt. During the legacy-message bridge the enriched result
may be 24 bytes, returning to the 16-byte target when plain `Result` reaches its final eight-byte layout.

## Consequences

Callers can distinguish validation, authentication, lifecycle, provider, capacity, and native failures without parsing
prose or allocating. Canonical English text remains in an optional formatter header and can be omitted or replaced for
translation. Generic AsyncStreams event boundaries retain the portable category/error identity but intentionally erase
library-specific detail and context.

Capturing native diagnostics requires explicit branches adjacent to each failed call. Linux cleanup must not overwrite
errno before capture, and nonnegative short transfers report actual bytes rather than stale errno. CommonCrypto and CNG
use their returned status values.

## Confirmation

Contract tests assert numeric stability, layout, trivial copying, context factories, same-domain and foreign
propagation, formatter behavior, and unknown values. Producer regressions cover deterministic validation, lifecycle,
authentication, invalid-ciphertext, overlap, capacity, and size-limit failures. Cryptography tests run on macOS, Linux,
and Windows in Debug and Release, and generated single-file libraries compile.

## Related

- [COMMON-0009 - Use Library-Owned Structured Result Errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [Result error category registry](../Common/result-error-categories.md)
- [Cryptography architecture](cryptography-architecture.md)
- [CRYPTOGRAPHY-0001](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [CRYPTOGRAPHY-0002](cryptography-0002-prefer-aead-and-keep-cbc-explicitly-legacy.md)
- [CRYPTOGRAPHY-0007](cryptography-0007-offer-openssl-alongside-linux-af-alg.md)
- [CRYPTOGRAPHY-0008](cryptography-0008-load-openssl-on-apple-and-windows.md)

## Sources

- [Cryptography public API](../../Libraries/Cryptography/Cryptography.h)
- [Cryptography implementation](../../Libraries/Cryptography/Cryptography.cpp)
- [Cryptography tests](../../Tests/Libraries/Cryptography/CryptographyTest.cpp)
