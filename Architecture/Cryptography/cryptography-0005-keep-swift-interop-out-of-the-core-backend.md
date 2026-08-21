# CRYPTOGRAPHY-0005 - Keep Swift Interoperability Out of the Core Backend

Status: Superseded in part by CRYPTOGRAPHY-0006
Date: 2026-08-20

CRYPTOGRAPHY-0006 supersedes the conclusion that Apple AES-GCM must remain unavailable. The decision not to introduce
Swift interoperability into the core backend remains accepted.

## Context

CryptoKit provides AES-GCM on Apple platforms, and the Swift compiler can generate a C++ header that exposes Swift
functions. Cryptography must also remain an allocation-free C++ library with no STL or extra build dependency, and its
amalgamated form must compile and link as a standalone C++ translation unit.

A Swift 6.2.4 feasibility probe successfully called a CryptoKit AES-GCM wrapper from C++. The generated header included
C++ standard-library and Swift interoperability headers, and its inline function called a Swift ABI symbol supplied by
a separately compiled Swift object. That object linked Foundation, CryptoKit, and the Swift runtime. CryptoKit's AES-GCM
interface also accepts and returns `Data`, rather than writing directly to caller-owned output spans.

## Decision

Do not use a generated Swift-to-C++ interface as the Apple backend of the core Cryptography library. Apple AES-GCM stays
unavailable through `queryFeatures()` until a public native API fits the existing library and standalone amalgamation
contracts, or those contracts are changed by a separate approved decision.

A future mixed-language adapter may wrap Cryptography outside the core library, but it is not part of the current
library or its single-file distribution.

## Consequences

The core library retains its existing build, dependency, and allocation boundaries. Apple continues to lack AES-GCM,
even though applications that already use Swift can call CryptoKit independently. The generated header is useful as a
binding for mixed Swift/C++ targets, but not as embeddable C++ implementation code.

## Confirmation

The decision is preserved when Cryptography and `SaneCppCryptography.h` contain no generated Swift header or Swift ABI
references, the Apple backend does not require `swiftc`, Foundation, or CryptoKit at build time, and Apple reports both
AES-GCM feature flags as unavailable.

## Related

- [CRYPTOGRAPHY-0001](cryptography-0001-wrap-native-symmetric-cryptography-with-capability-reporting.md)
- [CRYPTOGRAPHY-0006](cryptography-0006-compose-apple-gcm-over-commoncrypto-aes.md)
- [Swift C++ interoperability](https://www.swift.org/documentation/cxx-interop/)
- [Setting up mixed-language Swift and C++ projects](https://www.swift.org/documentation/cxx-interop/project-build-setup/)
- [Swift C++ interoperability constraints](https://www.swift.org/documentation/cxx-interop/status/)
