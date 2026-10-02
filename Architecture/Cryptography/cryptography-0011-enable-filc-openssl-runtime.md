# CRYPTOGRAPHY-0011 - Enable Fil-C OpenSSL Runtime

Status: Accepted
Date: 2026-10-03

## Context

The OpenSSL 3 backend already uses a narrow runtime-loaded EVP ABI, but was disabled under Fil-C. The project-maintained
Fil-C 0.685 compatibility patch is rebased onto upstream OpenSSL 3.6.5 and builds a shared `libcrypto.so.3`. ELF symbol
inspection shows instrumented exports with a `pizlonated_` prefix, but the Fil-C runtime's `dlsym` resolves the
unprefixed OpenSSL API names to the compatible entry points. A bounded runtime probe resolved all 25 names used by the
adapter unprefixed, while direct prefixed lookups failed; calling the plain `OPENSSL_version_major` pointer returned
3. The DSO's runtime dependencies must also be available through the Fil-C package loader path.

## Decision

Enable the existing OpenSSL backend on supported platforms, including Linux Fil-C, and use the existing Linux loader
with the ordinary unprefixed OpenSSL API names. Do not construct `pizlonated_` symbol names or bypass Fil-C's `dlsym`
mapping to select exported assembly entry points directly. Continue to select `Backend::Native` by default. Linux
native cryptography remains AF_ALG, with no automatic fallback between it and OpenSSL.

Run the existing Cryptography suite in a separate OpenSSL backend instance. The existing native/OpenSSL AES-GCM
differential runs when both backends report the required features. `SC_CRYPTOGRAPHY_TEST_REQUIRE_OPENSSL=1` makes the
OpenSSL instance require its expected capabilities when the Fil-C package DSO is selected.

The OpenSSL patch and Fil-C runtime package remain versioned dependency work: the current compatibility target is
Fil-C 0.685 with upstream OpenSSL 3.6.5. ABI checks and functional vectors establish interface compatibility and
cryptographic behavior for tested inputs; they do not establish constant-time execution, side-channel resistance, or
an independent security proof of OpenSSL or its Fil-C patch. The project Cryptography suite does not claim to replace
the full upstream OpenSSL test suite under Fil-C.

## Consequences

Fil-C can explicitly use userspace OpenSSL when its compatible package DSO and dependencies are available. Missing or
incompatible runtimes continue to produce an unavailable OpenSSL capability set. OpenSSL retains its internal
allocation behavior; the library does not become allocation-free end to end. AF_ALG behavior and default backend
selection do not change.

## Confirmation

Compile the OpenSSL ABI declarations against the packaged OpenSSL 3.6.5 headers and compile the Fil-C cryptography
translation units with the repository's strict warnings. Runtime confirmation requires the package loader path to
select the Fil-C DSO, all required unprefixed names to resolve through Fil-C `dlsym`, the separate OpenSSL test instance
to pass with `SC_CRYPTOGRAPHY_TEST_REQUIRE_OPENSSL=1`, and the differential section to run when native AF_ALG GCM is
available. These checks are functional and ABI evidence, not side-channel validation.

## Related

- [CRYPTOGRAPHY-0007 - Offer OpenSSL Alongside Linux AF_ALG](cryptography-0007-offer-openssl-alongside-linux-af-alg.md)
- [CRYPTOGRAPHY-0010 - Enable Fil-C AF_ALG Compatibility](cryptography-0010-enable-filc-af-alg-compatibility.md)
- [Fil-C OpenSSL 3.6.5 compatibility patch](../../Tools/Support/FilCOpenSSL365.patch)
