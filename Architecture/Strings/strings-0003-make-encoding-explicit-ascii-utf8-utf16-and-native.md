# STRINGS-0003 - Make Encoding Explicit: ASCII, UTF-8, UTF-16, and Native

Status: Accepted
Date: 2026-07-04

## Context

Sane C++ Libraries runs across POSIX and Windows APIs. POSIX paths and many text interfaces are UTF-8, while Win32 APIs commonly use UTF-16. Treating all strings as plain bytes or a single project-wide encoding would either lose native API fidelity or force unnecessary conversion.

## Decision

String primitives carry explicit encoding metadata for ASCII, UTF-8, UTF-16, and native platform encoding. String algorithms operate on code points where needed, not grapheme clusters. Native-facing APIs may preserve native encoding and convert only when a caller or operating-system API requires it.

## Consequences

Callers and reviewers can see when text is ASCII, UTF-8, UTF-16, or native. Windows and POSIX paths can be represented without hidden conversion. Advanced Unicode behavior such as grapheme cluster iteration, case conversion, and normalization remains separate work rather than an implicit guarantee of ordinary string iteration.

## Confirmation

A change preserves this decision when string and path views keep explicit encoding, conversions are visible in API calls or output configuration, UTF iterators avoid reading past valid ranges, and documentation continues to distinguish code-point operations from grapheme or normalization behavior.

## Related

- [COMMON-0008 - Keep StringSpan and StringPath in Common](../Common/common-0008-keep-stringspan-and-stringpath-in-common.md)
- [Strings documentation](../../Documentation/Libraries/Strings.md)
- [StringSpan](../../Libraries/Common/StringSpan.h)
- [StringView](../../Libraries/Strings/StringView.h)
- [StringIterator](../../Libraries/Strings/StringIterator.h)
