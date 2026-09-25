# COMMON-0010 - Assign Errors to Foundational StringSpan

Status: Accepted
Date: 2026-09-27

## Context

`StringSpan::appendNullTerminatedTo` is a shared, allocation-free native-output helper used by Process, Plugin,
FileSystemWatcher, and other libraries. Its old literal failures cannot be owned by the higher-level Strings library
without making Common depend on that library. Assigning a different category at every call site would also hide the
same underlying failure behind unrelated identities.

## Decision

The foundational `StringSpan` type owns built-in category 19 and a small append-only `StringSpanError` enum beside
its definition. This is a type-specific shared category, not a category for all Common code and not a repository-wide
list of library errors. It distinguishes an invalid destination offset, insufficient destination, native conversion
failure, and unsupported encoding. The Windows conversion helper currently reports malformed input and insufficient
output through one boolean; its primary code therefore honestly combines those causes rather than claiming to know
which one occurred.

Canonical English text lives only in optional `StringSpanErrorFormatter.h` and writes to caller-provided storage.
Libraries propagating this helper's failure preserve its category and code; callers that deliberately translate it
to their own higher-level operation error must do so explicitly. The `StringSpan.h` version guard advances because
the public error identity contract changes.

## Consequences

No Common-to-Strings dependency or allocation is added, and the Result remains a plain category/code carrier.
Applications can format or translate a native-output failure without linking a Common-wide message catalog.

## Confirmation

StringSpan tests exercise exact identities and optional formatting on macOS, Linux, and Windows. The category
registry verifies uniqueness, and single-file libraries compile after regeneration.

## Related

- [Common Result decision](common-0009-use-library-owned-structured-result-errors.md)
- [Keep StringSpan and StringPath in Common](common-0008-keep-stringspan-and-stringpath-in-common.md)
