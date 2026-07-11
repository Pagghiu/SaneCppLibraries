@page library_hashing Hashing

@brief 🟩 Incremental MD5, SHA-1 and SHA-256 hashing over byte spans

[TOC]

[SaneCppHashing.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHashing.h) is a small,
allocation-free adapter over the host operating system's hashing facilities. It is intended for code that receives a
byte stream in pieces—such as a file read through a fixed buffer—and needs one MD5, SHA-1 or SHA-256 digest without
collecting the complete input in memory.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Hashing.svg)


# When Hashing fits

Use `Hashing` for compatibility checksums, content identity and integrity checks where one of its three fixed algorithms
is the required wire or file format. The package tool is a representative use: it reads a download in 4 KiB chunks,
feeds each chunk to `Hashing`, converts the final digest to hexadecimal with [Strings](@ref library_strings), and compares
it with the expected value.

The algorithm choice is deliberately narrow. MD5 and SHA-1 remain useful when interoperating with existing formats, but
they are collision-broken and should not be selected for new security-sensitive designs. SHA-256 is the strongest option
provided here, but a plain digest does not authenticate who produced the data. This library does not provide HMAC,
password hashing, key derivation, signatures, encryption, a pluggable algorithm interface, or constant-time digest
comparison. Use a dedicated cryptographic library when any of those properties are part of the requirement.

# A hash is one incremental session

The mental model is a small state machine:

1. Construct `SC::Hashing`, then call `setType()` to select and initialize an algorithm.
2. Call `add()` zero or more times. Chunk boundaries do not become part of the digest; hashing `"test"` twice is the same
   byte stream as hashing `"testtest"` once.
3. Call `getHash()` to finalize into a caller-owned `SC::Hashing::Result`.

Treat the session as consumed after `getHash()`. Call `setType()` again—even with the same type—before hashing another
stream; that explicitly resets the platform state and gives portable behavior across the OS backends. `Hashing` is
move-disabled and copy-disabled because it owns that in-progress native state.

This compiled test shows the full sequence, including conversion of the binary result to printable hexadecimal:

\snippet Tests/Libraries/Hashing/HashingTest.cpp HashingSnippet

Every operation that can touch the native backend returns `bool`. Check `setType()`, every `add()`, and `getHash()`; a
failure means the requested operation was not completed. Calling `add()` or `getHash()` before a successful `setType()`
also fails. The API reports success or failure only—it does not expose an OS error code or diagnostic string.

# Storage, allocation and lifetime

`add()` borrows an `SC::Span<const uint8_t>` only for the duration of the call. The input buffer can therefore be reused
for the next file, socket or parser chunk as soon as `add()` returns. The library does not retain input bytes and does not
allocate a buffer proportional to the stream, so the total input size is not limited by the hashing object.

`SC::Hashing::Result` contains an inline 32-byte array plus the actual digest length: 16 bytes for MD5, 20 for SHA-1 and
32 for SHA-256. `Result::toBytesSpan()` is a view into that array, not an owning copy. The result object must outlive the
span, and hexadecimal or Base64 text requires a separate encoding step and caller-chosen storage. [Strings](@ref
library_strings) provides `StringBuilder::appendHex`; Hashing intentionally does not depend on it.

There is no dynamic allocation in the Hashing library itself. On Apple platforms the algorithm context lives inline in
the object. Windows and Linux implementations acquire native cryptographic handles or sockets, so construction and
`setType()` can still consume fallible OS resources; destruction releases them. A `Hashing` instance and its active
session should stay on one thread unless the caller supplies its own synchronization.

# Platform and integration boundaries

The same public workflow maps to CommonCrypto on Apple platforms, CryptoAPI on Windows, and the kernel `AF_ALG` hashing
interface on Linux. Other targets currently compile a stub whose operations return `false`; support should therefore be
verified for the deployment target rather than inferred from the header being available. Backend selection is an
implementation detail, but it explains why the API is fallible even though the object and digest storage are fixed-size.

Hashing sits below the libraries that own I/O and presentation policy:

- [File](@ref library_file) or [Async](@ref library_async) supplies chunks; Hashing neither opens files nor schedules I/O.
- [Strings](@ref library_strings) can encode a digest for logs, manifests or comparison; Hashing returns binary bytes.
- [Build](@ref page_build) uses Hashing while checking downloaded package archives, but Hashing has no dependency on the
  build or package tooling.

The separate [C bindings](@ref group_sc_hashing) expose the same initialize/add/finalize lifecycle with caller-owned
opaque storage. C callers must pair `sc_hashing_init()` with `sc_hashing_close()` so native resources are released.

For exact types and signatures, see the [Hashing API group](@ref group_hashing).

# Status

🟩 Usable

The library is intentionally small and is used by the package tooling. Its main limitations are the fixed legacy-oriented
algorithm set, boolean-only error reporting, and lack of a supported backend outside Apple, Windows and Linux.

# Roadmap

💡 Unplanned Features:

- No additions are currently planned. New cryptographic primitives should be driven by a concrete security model rather
  than added as algorithm names alone.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Hashing`.
Single File counts
`SaneCppHashing.h`.
Standalone counts `SaneCppHashingStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 95		| 292		| 387	|
| Single File | 404		| 372		| 776	|
| Standalone  | 404		| 372		| 776	|
