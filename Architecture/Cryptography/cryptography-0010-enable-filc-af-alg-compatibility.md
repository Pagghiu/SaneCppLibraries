# CRYPTOGRAPHY-0010 - Enable Fil-C AF_ALG Compatibility

Status: Accepted
Date: 2026-10-03

## Context

The existing Linux AF_ALG implementation was compile-disabled under Fil-C. A native Fil-C 0.685 probe on Linux
6.8.0-137 showed that socket, bind, key, accept, ancillary data, CBC, HMAC, and GCM operations work, but the original
`ALG_SET_AEAD_AUTHSIZE` call passed a null pointer with a length of 16. Fil-C trapped in `zsys_setsockopt` before the
kernel syscall. Passing a valid 16-byte scratch buffer let the kernel option and GCM vectors succeed.

This is a compatibility issue, not a security assessment. The [current Linux kernel documentation](https://www.kernel.org/doc/html/latest/crypto/userspace-if.html)
labels AF_ALG insecure and deprecated, retained for backwards compatibility, and records default algorithm restrictions
starting in Linux 7.3.

## Decision

Enable the existing AF_ALG implementation on Linux under Fil-C. For `ALG_SET_AEAD_AUTHSIZE`, pass a local 16-byte
scratch buffer while preserving `GCMTagSize` as the `setsockopt` length. Keep feature discovery runtime-based: report
only algorithms whose existing AF_ALG capability probes succeed, and keep operation tests conditional on those
reported features.

This change adds no backend, dependency, fallback, or public API. Backend selection and default semantics do not
change. Keep the OpenSSL 3 Fil-C guard; OpenSSL 3 remains the preferred future route for kernel-independent
cryptography work, but enabling it under Fil-C is outside this decision.

## Consequences

Fil-C builds can use the existing native Linux backend when the running kernel exposes the required algorithms.
Feature values may differ across kernels and configurations, including kernels with AF_ALG algorithm restrictions.
Passing known-answer tests on the Linux 6.8 VM establishes correctness for that test environment only. It does not
establish AF_ALG security or suitability for new security-sensitive deployments; AF_ALG's compatibility status and
upstream security warning remain material limitations.

## Confirmation

The auth-size scratch buffer is used by both `aeadSupported()` and AEAD initialization. Cryptography tests do not
hard-code Fil-C's symmetric feature set; native operations remain gated by runtime feature results, and validation
errors retain their normal identities. Confirm with focused Cryptography tests and native Fil-C Debug/Release
`SCTest`, plus the existing macOS and Windows checks. OpenSSL remains disabled under Fil-C and no fallback is added.

## Related

- [CRYPTOGRAPHY-0001 - Native capability reporting](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [CRYPTOGRAPHY-0007 - OpenSSL alongside Linux AF_ALG](cryptography-0007-offer-openssl-alongside-linux-af-alg.md)
- [Linux AF_ALG userspace interface and deprecation notice](https://www.kernel.org/doc/html/latest/crypto/userspace-if.html)

## Sources

- [Cryptography implementation](../../Libraries/Cryptography/Cryptography.cpp)
- [Cryptography tests](../../Tests/Libraries/Cryptography/CryptographyTest.cpp)
