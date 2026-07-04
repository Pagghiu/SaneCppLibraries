# SERIALIZATIONTEXT-0005 - Treat Borrowed StringSpans As Raw JSON String Slices

Status: Accepted
Date: 2026-07-04

## Context

JSON string escapes may require producing different bytes from the source token. Owning string-like destinations can receive unescaped output through caller-provided storage, but borrowed views such as `StringSpan` and `StringView` do not own storage where unescaped bytes can be written.

## Decision

When deserializing JSON strings into borrowed string spans or views, Serialization Text accepts only raw token slices that contain no JSON escape sequences. When deserializing into mutable string-like destinations, the reader unescapes into the destination through a growable-buffer adapter.

## Consequences

Borrowed string reads remain allocation-free and lifetime-transparent because they point into the original JSON text. Escaped strings require an owning or writable destination. This makes the storage requirement explicit instead of silently allocating temporary unescaped text.

## Confirmation

A change preserves this decision when `StringSpan` and `StringView` JSON reads reject escaped source strings, owning or mutable string-like reads can unescape through caller-owned storage, and tests cover both borrowed raw strings and rejected escaped borrowed strings.

## Related

- [SerializationJson Reader](../../Libraries/SerializationText/SerializationJson.h)
- [SerializationJson implementation](../../Libraries/SerializationText/SerializationJson.cpp)
- [SerializationJson borrowed string tests](../../Tests/Libraries/SerializationText/SerializationJsonTest.cpp)
- [SERIALIZATIONTEXT-0003 - Use IGrowableBuffer transactional output instead of Strings/Memory](serializationtext-0003-use-igrowablebuffer-transactional-output-instead-of-strings-memory.md)
