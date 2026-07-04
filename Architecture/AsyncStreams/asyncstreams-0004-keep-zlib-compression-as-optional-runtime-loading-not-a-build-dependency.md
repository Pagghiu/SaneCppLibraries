# ASYNCSTREAMS-0004 - Keep ZLib Compression As Optional Runtime Loading, Not A Build Dependency

Status: Accepted
Date: 2026-07-04

## Context

Compression transforms are useful for stream pipelines, but linking zlib as a normal build dependency would conflict with the dependency-free shape of `AsyncStreams`. At the same time, platform zlib libraries are commonly available and can be used when explicitly loaded.

## Decision

`AsyncStreams` keeps ZLib compression support as an optional runtime-loaded facility. `ZLibAPI` loads platform zlib symbols dynamically, reports failures through `Result`, and keeps system headers and dynamic-loading details in internal implementation files instead of making zlib a build dependency.

## Consequences

Compression users must handle runtime load failures and platform library availability. The core library remains dependency-free for builds and single-file consumption, while compression transforms can still be composed when the runtime zlib library is present.

## Confirmation

A change preserves this decision when `AsyncStreams` build metadata does not add a zlib dependency, zlib loading remains explicit and failure-reporting, platform dynamic-loading headers stay out of public stream headers, and compression tests cover symbol loading and transform behavior.

## Related

- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [ASYNCSTREAMS-0001 - Keep AsyncStreams dependency-free through templated Async adapters](asyncstreams-0001-keep-asyncstreams-dependency-free-through-templated-async-adapters.md)
- [ZLibAPI](../../Libraries/AsyncStreams/Internal/ZLibAPI.h)
- [ZLib transform streams](../../Libraries/AsyncStreams/ZLibTransformStreams.h)
