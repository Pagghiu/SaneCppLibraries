@page library_cryptography Cryptography

@brief 🟥 Fixed-storage wrappers over native and optional OpenSSL symmetric cryptography APIs

[TOC]

[SaneCppCryptography.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppCryptography.h)
provides synchronous, caller-buffered access to secure random bytes, AES-GCM, HMAC, HKDF, and legacy AES-CBC with
PKCS#7 padding. The default `Native` backend delegates to operating-system providers. Apple, Windows, and Linux callers
can explicitly select a runtime-loaded OpenSSL 3 backend instead. On Apple, the native backend implements the narrowly
scoped SP 800-38D GCM composition over CommonCrypto AES because CommonCrypto has no public GCM interface. No portable
AES implementation is included.

# Status and security notice

🟥 **Draft / experimental**

The API, storage sizes, and platform coverage may change while the library is in Draft. The implementation has NIST
known-answer, deterministic mutation, and cross-platform tests, but it has **not received a formal cryptographic or
side-channel audit**. No formal audit has been performed or is required for Draft publication; users should judge the
library accordingly. Treat it as a low-level primitive adapter, not as a complete protocol or a substitute for expert
review of a security design.

Prefer an authenticated-encryption mode such as AES-GCM whenever it is available. Use AES-CBC only when an existing
protocol requires that exact construction; CBC encryption by itself does not authenticate ciphertext and is malleable.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Cryptography.svg)


# Choosing a primitive

| Requirement | API | Important boundary |
|:------------|:----|:-------------------|
| Generate keys, nonces, IVs, or salts | `SC::Cryptography::Random` | Random bytes still need protocol-specific length and reuse rules |
| Encrypt and authenticate one message | `SC::Cryptography::Aead` | AES-GCM is available on Apple, Windows, and capable Linux kernels; never reuse a nonce with the same key |
| Authenticate a byte stream | `SC::Cryptography::Hmac` | Compare MACs with a constant-time comparison supplied by the protocol/application |
| Derive independent keys | `SC::Cryptography::Hkdf` | `info` should identify the protocol, purpose, and key role |
| Interoperate with legacy AES-CBC | `SC::Cryptography::Cipher` | CBC is unauthenticated; use only inside a correctly specified authenticated protocol |

This library does not provide password hashing, signatures, public-key cryptography, certificate validation, key
storage, nonce allocation, protocol framing, or constant-time comparison.

# Capability discovery

Backend availability is part of the API. The overload without a backend always queries `Backend::Native`:

```cpp
SC::Cryptography::Features features;
SC_TRY(SC::Cryptography::queryFeatures(features));
if (features.aes256Gcm)
{
    // The AES-256-GCM backend is available in this process.
}
```

Applications that prefer OpenSSL can query and select it explicitly:

```cpp
SC::Cryptography::Features openSSLFeatures;
SC_TRY(SC::Cryptography::queryFeatures(SC::Cryptography::Backend::OpenSSL, openSSLFeatures));
if (openSSLFeatures.aes256Gcm)
{
    SC::Cryptography::Aead aead(SC::Cryptography::Backend::OpenSSL);
    // aead.init(...), aead.seal(...), or aead.open(...)
}
```

Default constructors, the original `queryFeatures()` overload, and the original `Hkdf::derive()` overload all use
`Backend::Native`. On Linux this means AF_ALG. `Aead`, `Cipher`, and `Hmac` bind their backend at construction;
`Hkdf::derive()` has a backend-taking overload because it constructs HMAC sessions internally. OpenSSL remains
optional on every platform: if a suitable OpenSSL 3 shared library cannot be loaded, its cryptographic features report
unavailable without affecting the native backend.

`queryFeatures()` reports primitive availability, not whether a chosen key, nonce, IV, message size, or surrounding
protocol is secure. `maximumAeadAssociatedDataSize` is zero when AES-GCM is unavailable or when the active backend has
no AAD-specific limit below its general maximum message size. Linux `Backend::Native` reports 4096 when either AF_ALG
AES-GCM variant is available; `Backend::OpenSSL` reports zero because it has no AF_ALG-specific AAD ceiling.

# Secure random bytes

`Random::fill()` fills the complete caller-owned span or returns an error. An empty span succeeds. Use it for secret
keys and for protocol values that are required to be random; do not use predictable counters where a specification
requires an unpredictable IV.

The compiled test demonstrates the call:

\snippet Tests/Libraries/Cryptography/CryptographyTest.cpp CryptographyRandomSnippet

Random output must not be tested for “random-looking” patterns in application code. The test's non-zero assertion is
only a smoke check that the backend wrote the buffer.

# AES-GCM authenticated encryption

`Aead` binds one AES-128 or AES-256 key with `init()`, then encrypts or decrypts independent messages with `seal()` and
`open()`. Ciphertext has the same length as plaintext. This API fixes the nonce at 12 bytes and the authentication tag
at 16 bytes. Every `init()` attempt discards the previous key, including when initialization fails.

For every message encrypted under one key:

- the nonce must be unique; nonce reuse can destroy GCM's confidentiality and authentication guarantees;
- AAD is authenticated but is not encrypted, and the same AAD must be supplied to `open()`;
- the Linux AF_ALG backend accepts at most 4096 bytes of AAD because patched kernels require an AAD-sized writable
  destination, which the allocation-free adapter provides on the stack;
- `ciphertext` needs at least `plaintext.sizeInBytes()` bytes and `tag` needs exactly 16 bytes;
- `plaintext` needs at least `ciphertext.sizeInBytes()` bytes;
- exact in-place input/output is supported by the current GCM backend, but partial overlap is rejected;
- writable outputs must not overlap nonce or AAD, and the tag must not overlap any other argument;
- `bytesWritten` is zero on every failure; validation failures leave output spans unchanged;
- after a sealing backend failure, ciphertext and tag must be treated as unusable;
- an authentication failure returns `bytesWritten == 0` and clears the supplied plaintext output span.

This known-answer test shows the complete operation. Zero keys and nonces are appropriate only for test vectors—real
callers must generate and manage them according to their protocol:

\snippet Tests/Libraries/Cryptography/CryptographyTest.cpp CryptographyAeadSnippet

See [NIST SP 800-38D](https://doi.org/10.6028/NIST.SP.800-38D) for GCM requirements and usage bounds.

The Apple adapter uses CommonCrypto for every AES block operation and implements counter construction, GHASH, and tag
verification privately. It supports only the fixed nonce and tag sizes above. GHASH uses fixed-iteration multiplication
without secret-indexed lookup tables, and `open()` authenticates ciphertext before decrypting it. Shared tests include
official NIST CAVP vectors plus deterministic boundary and corruption stress for both key sizes.
The vectors come from the
[NIST GCM validation corpus](https://csrc.nist.gov/Projects/Cryptographic-Algorithm-Validation-Program/CAVP-TESTING-BLOCK-CIPHER-MODES).

# HMAC

`Hmac` is an incremental HMAC-SHA256 or HMAC-SHA384 session:

1. Call `setType()` to select and reset the hash family. Every attempt discards any active computation, even on failure.
2. Call `setKey()` to install a key and start a fresh message.
3. Call `add()` zero or more times; chunk boundaries do not affect the MAC.
4. Call `getMac()` to finalize into `MacResult`.

`getMac()` consumes the running session. Call `setKey()` again before authenticating another message. `MacResult`
contains 48 inline bytes plus the actual size; `toBytesSpan()` borrows those bytes and must not outlive the result.
Call `reset()` to abandon an active computation without producing a MAC; it clears library-owned state while preserving
the selected hash family, so a new computation still requires `setKey()`.

\snippet Tests/Libraries/Cryptography/CryptographyTest.cpp CryptographyHmacSnippet

The snippet is an RFC known-answer test. Production code should use a secret key of the size and origin required by its
protocol and should compare received MACs in constant time.

# HKDF

`Hkdf::derive()` implements RFC 5869 extract-and-expand with HMAC-SHA256 or HMAC-SHA384. Output is written directly to
the caller span and is limited to `255 * HashLen` bytes. An empty salt means the RFC-defined `HashLen` zero octets; an
empty output span succeeds.

\snippet Tests/Libraries/Cryptography/CryptographyTest.cpp CryptographyHkdfSnippet

Use `info` for domain separation: encode the protocol version, algorithm suite, application context, and purpose of the
derived key. HKDF is not a password hash and does not make low-entropy passwords suitable key material. See
[RFC 5869](https://www.rfc-editor.org/rfc/rfc5869) for the construction and guidance.

# Legacy AES-CBC with PKCS#7

`Cipher` exposes AES-128-CBC and AES-256-CBC solely for compatibility with protocols that already specify CBC, PKCS#7,
IV generation, and authentication. New designs should use AEAD.

For encryption, the 16-byte IV must be unpredictable and freshly generated for each message. Reusing an IV leaks
relationships between plaintext prefixes. For decryption, do not reveal padding failures differently from other
authentication or message failures; doing so can create a padding oracle.

The streaming lifecycle is:

1. Call `start()` with the algorithm, operation, key, and a 16-byte IV. Every attempt discards the previous operation,
   even on failure.
2. Call `update()` with any chunk size. Input and output must not overlap.
3. Call `finish()` exactly once to emit or validate PKCS#7 padding.

For every `update()`, an output span of `input.sizeInBytes() + 15` bytes is sufficient regardless of buffered state.
`finish()` requires 16 output bytes. An insufficient output span fails before consuming input, so the same call can be
retried with more storage. Successful `finish()`, invalid ciphertext length, invalid padding, or a native processing
failure consumes the session; call `start()` before reuse.

Call `reset()` to abandon an operation before `finish()`. It is idempotent and releases native resources plus buffered
key, IV, block, and padding state owned by the library.

\snippet Tests/Libraries/Cryptography/CryptographyTest.cpp CryptographyCbcSnippet

Again, the zero values in this snippet are a known-answer vector, not acceptable production key or IV generation.

# Composing CBC And HMAC With Async Streams

[Async Streams](@ref library_async_streams) provides header-only `AsyncCipherTransformStreamT` and
`AsyncHmacWritableStreamT` adapters. They are templates specifically so Cryptography remains synchronous and neither
library acquires a hard dependency on the other. Include both headers only in the application translation unit that
builds the pipeline.

The cipher adapter inherits all CBC warnings above. The HMAC adapter is a sink rather than a pass-through transform;
use pipeline fan-out when the same bytes must also reach a file, socket, or another destination. The Async Streams
documentation shows complete setup and buffer requirements.

There is no AES-GCM stream adapter. The one-shot `Aead` interface deliberately keeps authenticated messages explicit;
streaming it safely requires a separately specified record protocol that withholds plaintext until its tag verifies.

# Storage, allocation, and secret lifetime

All public objects hold provider state in fixed inline opaque storage. Public operations borrow input spans only for the
duration of the call and write to caller-owned output spans. The library performs no C++ heap allocation and has no STL,
exception, or RTTI dependency. The Linux AF_ALG backend uses fixed Sane storage and kernel resources and does not require
or load OpenSSL.

Selecting `Backend::OpenSSL` changes that end-to-end allocation contract. OpenSSL allocates provider, algorithm,
cipher/MAC context, process, and thread state internally; Cryptography cannot redirect or prevent those allocations.
Capability queries, session initialization, and provider operations can therefore allocate inside OpenSSL even though
Sane C++ continues to use fixed inline storage and caller-owned output buffers.

`Aead`, `Cipher`, and `Hmac` are non-copyable and non-movable because each owns live native state. Keep an active object
on one thread unless the caller provides external synchronization. Destruction releases native handles. Fixed native
key-object buffers, CBC pending blocks, HMAC contexts, and HKDF intermediate key material are cleared when their session
ends; copies held by the operating system remain governed by that platform API.

The library does not lock caller key buffers into RAM, prevent callers from copying secrets, or guarantee that every
compiler, kernel, crash dump, swap configuration, or debugger removes all historical copies. Applications with stronger
key-erasure requirements need platform-specific memory and process controls in addition to this API.

# Platform support

`queryFeatures()` is the runtime source of truth.

| Platform target | Secure random | AES-CBC PKCS#7 | AES-GCM | HMAC / HKDF |
|:----------------|:--------------|:----------------|:--------|:------------|
| macOS 13+ | CommonCrypto | AES-128 / AES-256 | AES-128 / AES-256 over CommonCrypto AES | SHA-256 / SHA-384 |
| macOS, `OpenSSL` | CommonCrypto | OpenSSL 3 AES-128 / AES-256 CBC | OpenSSL 3 AES-128 / AES-256 GCM | OpenSSL 3 HMAC SHA-256 / SHA-384 |
| Windows 10+ | CNG | AES-128 / AES-256 | AES-128 / AES-256 | SHA-256 / SHA-384 |
| Windows, `OpenSSL` | CNG | OpenSSL 3 AES-128 / AES-256 CBC | OpenSSL 3 AES-128 / AES-256 GCM | OpenSSL 3 HMAC SHA-256 / SHA-384 |
| Linux, `Native` | `getrandom()` | `AF_ALG cbc(aes)` when available | `AF_ALG gcm(aes)` when available | `AF_ALG hmac(sha256)` / `hmac(sha384)` when available |
| Linux, `OpenSSL` | `getrandom()` | OpenSSL 3 AES-128 / AES-256 CBC | OpenSSL 3 AES-128 / AES-256 GCM | OpenSSL 3 HMAC SHA-256 / SHA-384 |

Linux capability reporting probes the running kernel. A Linux build can therefore compile successfully while a
particular primitive reports unavailable at runtime. AES-GCM additionally requires the kernel's AF_ALG AEAD interface
(commonly the `algif_aead` module) to be loaded or available for automatic module loading. Unsupported platforms compile
stubs that return errors and report all features as unavailable.

Use `algif_aead` only on a security-updated kernel. The interface was affected by
[CVE-2026-31431](https://ubuntu.com/security/CVE-2026-31431), and some distributions disable its module on affected
kernels. `queryFeatures()` can detect whether the interface is usable, but it cannot prove that a vendor kernel contains
all relevant security fixes.

The optional OpenSSL backend loads only an OpenSSL 3 `libcrypto` ABI, requires no OpenSSL headers or link-time
`libcrypto` dependency, and probes actual provider algorithm initialization. Linux probes `libcrypto.so.3`. macOS first
uses the dynamic loader's search paths for `libcrypto.3.dylib`, then checks the conventional Homebrew and MacPorts
locations. Windows probes the architecture-specific OpenSSL 3 DLL name and `libcrypto-3.dll` through the safe default
DLL directories. It does not search the process working directory.

The backend uses the process default OpenSSL library context without loading providers or changing policy. Missing
libraries, incomplete symbols, configuration/FIPS policy, or unavailable algorithms are reflected in that backend's
feature result and do not affect the native backend.

# Error handling

Every operation returns `SC::Result`. Check every result and propagate failures with `SC_TRY`; do not continue with
partially produced ciphertext, plaintext, MAC, or derived key material. Error strings are stable diagnostic categories,
not portable native error codes and not suitable as protocol responses.

# Roadmap

Current Draft goals:

- keep the API small, keep Sane code and the AF_ALG adapter allocation-free, and make OpenSSL allocations explicit;
- validate every advertised primitive on macOS, Windows, and Linux;
- keep the Apple GCM composition narrow, directly traceable to SP 800-38D, and covered by shared known-answer tests;
- consider a C binding only after the C++ lifecycle and storage contract stabilizes;
- keep the absence of a formal audit explicit and use sustained testing, real-world use, and focused community feedback
  to inform any future maturity change.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Cryptography`.
Single File counts
`SaneCppCryptography.h`.
Standalone counts `SaneCppCryptographyStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 494		| 2474		| 2968	|
| Single File | 635		| 2912		| 3547	|
| Standalone  | 635		| 2912		| 3547	|
