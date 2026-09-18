# FILESYSTEM-0003 - Use Structured FileSystem Result Errors

Status: Accepted
Date: 2026-09-21

## Context

FileSystem currently represents validation, path, mutation, metadata, copy, and native failures with English string
literals. POSIX failures sometimes expose errno names as identity while equivalent Windows failures collapse to generic
text. The optional precise-error mode formats text into mutable instance storage and returns a Result that borrows that
storage, so later operations can invalidate a previously returned diagnostic.

Predicate operations also use failed Results for ordinary false answers such as a missing entry or an unexpected entry
type. That prevents low-level callers from separating expected query outcomes from genuine validation or native errors.

## Decision

FileSystem owns category 11 and one portable `FileSystemError` taxonomy. Primary errors describe caller-visible
conditions such as a missing entry, denied access, exhausted storage, unsupported operation, or incomplete transfer.
Operating-system names, native API names, and raw errno constants are forbidden in primary errors.

`FileSystemErrorDetail` identifies the logical operation or backend stage. A detail may identify a platform mechanism
only when it materially explains a failure and its enum name explicitly qualifies the platform. The
`WindowsQueryReparsePoint` detail is retained because reading a Windows link can fail while querying a reparse point,
which is distinct from opening the link or storing its target. This platform distinction is diagnostic only and never
changes the portable primary identity. Raw native error numbers belong in `FileSystemErrorContext`, tagged as
`NativeError`; capacities and transfer counts use separate typed context tags.

`ResultFileSystem` composes the authoritative Result with fixed-width detail and context tags and one 32-bit scalar
payload. It owns no memory and retains no borrowed strings. Plain-result conversion preserves category/error identity
and deliberately loses FileSystem detail and context. Canonical English text lives only in the optional formatter
header; applications may omit it or provide translations.

The `preciseErrorMessages` switch, internal formatting buffer, allocation-based Windows message formatting, and
borrowed formatted Result are removed during the port. A Result never retains a path string. Callers already own the
input paths and can combine them with the stable error, detail, and scalar context when presenting diagnostics.

The shared `Common/WindowsPath.inl` fragment remains category-neutral. FileSystem translates its internal path status
at the implementation boundary into FileSystem-owned errors, details, and native context.

Fallible action APIs return `ResultFileSystem`. Predicates offer `ResultFileSystem` plus a `bool&` output: a missing
entry, mismatched type, or denied access is a successful false answer, while validation and unexpected native failures
remain inspectable errors. Existing one-argument boolean queries remain compact and intentionally lossy conveniences.
The action-like `moveDirectory` facade returns `ResultFileSystem` so its failure is inspectable while existing truth
checks remain source-compatible.

The exported return-type changes are an intentional ABI transition on the unintegrated result branch and require
clients to rebuild. During the legacy-message bridge the enriched result may be 24 bytes, returning to the 16-byte
target when plain Result reaches its final eight-byte layout.

## Consequences

Callers can branch on stable cross-platform conditions and inspect the failed stage and native code without parsing
prose or allocating. Equivalent Windows and POSIX failures share primary identities. Formatting and translation are
pay-for-use facilities and cannot mutate or invalidate a returned result.

Native diagnostic values must be captured immediately after a failed system call, before cleanup, retry, formatting,
or other calls can overwrite them. Byte contexts are attached only when exactly representable and are never truncated.
Behavior corrections discovered during the migration require focused regression tests rather than being folded into a
mechanical string replacement.

## Confirmation

Contract tests assert numeric stability, layout, trivial copying, context factories, same-domain and foreign
propagation, formatter behavior, and unknown values. Producer tests cover deterministic validation, condition, and
native failures on macOS, Linux, and Windows. Predicate tests distinguish expected false outcomes from actual failures.
Generated single-file libraries continue to compile.

## Related

- [COMMON-0009 - Use Library-Owned Structured Result Errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [Result error category registry](../Common/result-error-categories.md)
- [FileSystem architecture](filesystem-architecture.md)
- [FILESYSTEM-0001](filesystem-0001-resolve-relative-operations-against-an-explicit-base-directory.md)
- [FILESYSTEM-0002](filesystem-0002-keep-filesystem-dependency-free-even-for-file-like-operations.md)

## Sources

- [FileSystem public API](../../Libraries/FileSystem/FileSystem.h)
- [FileSystem implementation](../../Libraries/FileSystem/FileSystem.cpp)
- [Shared Windows path implementation](../../Libraries/Common/WindowsPath.inl)
- [FileSystem tests](../../Tests/Libraries/FileSystem/FileSystemTest.cpp)
