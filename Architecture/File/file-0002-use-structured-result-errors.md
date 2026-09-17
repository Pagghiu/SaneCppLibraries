# FILE-0002 - Use Structured File Result Errors

Status: Accepted
Date: 2026-09-21

## Context

File previously represented descriptor, path, anonymous-pipe, and named-pipe failures with English string literals.
Equivalent operations used unrelated text on Windows and POSIX, callers could not reliably inspect failures, and every
consumer retained the diagnostic strings even when it never displayed an error.

File also shares the private `Common/WindowsPath.inl` implementation fragment with FileSystem and Testing. Common must
not acquire the error taxonomy of any of those libraries merely because their implementations share path mechanics.

## Decision

File owns category 10 and one portable `FileError` taxonomy. Primary errors describe caller-visible conditions and
logical operations. Operating-system names and native API names are forbidden in primary errors.

`FileErrorDetail` identifies the logical operation or backend stage. Details may name an operating system or provider
only when the distinction materially aids diagnosis and the name is explicitly qualified. Native error numbers belong
in `FileErrorContext`, tagged as `NativeError`; capacity and transfer information use separate typed context tags.

`ResultFile` composes the authoritative `Result` with fixed-width detail and context tags and one 32-bit scalar payload.
It owns no memory and retains no borrowed strings. Plain-result conversion preserves category/error identity and
deliberately loses File detail and context. Canonical English text lives only in the optional formatter header.

The shared Windows path fragment exposes a small category-neutral internal status. Each including library translates
that status at its own implementation boundary. It does not return a File or FileSystem error, and it does not embed
legacy prose. This keeps library taxonomies local while avoiding a policy template threaded through path algorithms.

End of file remains successful completion reported by a zero-sized `actuallyRead` span. It is never represented as an
error. Short writes are explicit `IncompleteWrite` failures and carry the exactly representable actual byte count.

The exported File APIs return `ResultFile` so domain-aware callers retain diagnostics. This changes binary ABI during
the unintegrated result branch; clients must be rebuilt. During the legacy-message bridge the enriched result may be
24 bytes, returning to the 16-byte target when plain `Result` reaches its final eight-byte layout.

## Consequences

Callers can distinguish validation, lifecycle, I/O, capacity, pipe, and native failures without parsing prose or
allocating. Formatters can be omitted from executables and replaced by application-local translations.

Native diagnostic capture must occur immediately after a failed system call. Cleanup, retry, and result construction
must not overwrite the value first. Sizes are attached only when exactly representable and are never truncated.

## Confirmation

Contract tests assert numeric stability, layout, trivial copying, context factories, same-domain and foreign
propagation, formatter behavior, and unknown values. Producer tests cover deterministic validation and state failures,
with native I/O behavior tested on macOS, Linux, and Windows. Generated single-file libraries continue to compile.

## Related

- [COMMON-0009 - Use Library-Owned Structured Result Errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [Result error category registry](../Common/result-error-categories.md)
- [File architecture](file-architecture.md)
- [FILE-0001](file-0001-represent-named-pipes-as-file-pipe-descriptors.md)

## Sources

- [File public API](../../Libraries/File/File.h)
- [File implementation](../../Libraries/File/File.cpp)
- [Shared Windows path implementation](../../Libraries/Common/WindowsPath.inl)
- [File tests](../../Tests/Libraries/File/FileTest.cpp)
