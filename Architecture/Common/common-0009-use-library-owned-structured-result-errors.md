# COMMON-0009 - Use Library-Owned Structured Result Errors

Status: Accepted
Date: 2026-09-12

## Context

`Result` currently stores one stable ASCII message pointer. This keeps the value small and makes failure propagation
simple, but programs must inspect prose to distinguish failures, every error literal can become executable data, and
variable diagnostics require borrowed formatted buffers. The wording is inconsistent and cannot be translated without
replacing the error identity itself.

Some operations also have useful failure context such as a byte offset, required capacity, operation kind, or native
error number. Storing every possible detail in `Result` would make a cross-library type large and would impose the cost
on callers that only need success or failure.

## Decision

The final `Result` representation is one 64-bit structured identity containing a 32-bit category value and a 32-bit
error value. Zero/zero represents success. Category zero is reserved for uncategorized errors, and `Result(false)`
represents uncategorized/unspecified so boolean propagation remains supported. `Result` remains trivially copyable,
standard-layout, non-owning, allocation-free, and eight bytes.

A library's primary error enum describes failures at the portable public-API or library-operation level. Equivalent
logical failures use the same primary error on every supported platform; native API names, backend implementation
steps, and other platform-specific distinctions do not normally belong in the primary identity. This keeps code that
branches on a plain `Result` independent of the backend that produced it.
Project-tool failures are likewise owned by their tool subsystem, never added to a library's enum solely because tools
use that library; see [TOOLS-0001](../Tools/tools-0001-own-tool-error-identities.md).

`ResultCategory` is an open fixed-width value type defined in Common. Error categories and error enums are declared by
the library that owns them, not collected in Common. Built-in category numbers are centrally assigned, append-only,
and collision-checked in the [Result error category registry](result-error-categories.md). A reserved numeric range is
available for application and external-library categories.
Each owner keeps its error enum, category, detail/context types, and enriched result (when present) in a dedicated
`<Owner>Error.h`. The ordinary API header includes that file for source compatibility; optional formatter headers
include the error header directly rather than pulling in the whole API.
The foundational `StringSpan` source fragment is a narrow exception: it owns its own type-specific category and enum
in its guarded `StringSpanError.h` because Common cannot depend on Strings and several libraries propagate its native-output
failures. This does not create a Common-wide error catalogue or collect any library's errors centrally.

A library may define a composed enriched result when callers benefit from structured context. Its first-class status is
still represented by `Result`; additional fields contain only copied scalar values, enums, offsets, sizes, or native
error numbers. When lower-level failure stages are useful, the library defines a public stable detail enum and stores it
in the enriched result. Platform-specific detail values explicitly identify their platform where the distinction is
not portable. Detail is available for programmatic inspection and formatting, but is not part of the plain `Result`
identity. Once released, primary and detail numeric assignments are append-only; removed values remain reserved. The
composed `Result` is the only source of truth for success and primary error identity; a local primary error enum is
derived only when its category matches and is not duplicated as mutable state. Foreign-category failures remain valid
inside an enriched result with inactive context cleared. Enriched results do not own memory or allocate. Borrowed
immutable text is prohibited by default and requires a library-specific ADR that identifies the owner and proves the
lifetime contract. Converting an enriched result to plain `Result` preserves category and primary error identity and
deliberately discards detail and other context. Enriched results target at most 16 bytes; a larger public result needs a
library-specific ADR and supported-platform ABI evidence.

A platform-specific primary error is allowed only when the platform distinction is itself part of the caller-visible
semantics and no honest portable operation-level identity exists. Such an error includes the platform name in its enum
value and requires explicit justification in an ADR. Merely calling a platform-specific API, or wanting to retain the
stage at which it failed, is not sufficient justification; those facts belong in the enriched detail and native error
fields.

Canonical English messages and variable formatting are presentation facilities, not result state. Mandatory library
headers contain no error-message literals. Optional library-owned formatters follow the
[Result error formatting contract](result-error-formatting.md), write to caller-provided storage, and may be replaced by
applications that translate the public category, error, and context fields. Formatters are inline implementations in
separate opt-in headers, not exported from mandatory shared libraries. Static and shared core artifacts therefore do
not acquire canonical text; the application instantiates only the formatting it requests. Release binary-elision
experiments on macOS, Linux, and Windows confirm this boundary, including unused single-file formatters. This is not
a promise that arbitrary unoptimized builds or externally forced symbol retention will discard unused inline text.

Migration used a temporary unreleased bridge containing both the legacy message pointer and numeric identity.
The completed representation removes the message pointer, literal/stable-pointer factories, bridge predicates, and
`SC_TRY_MSG`. The versioned Common guard rejects mixing old and new copies in one translation unit. All participating
binaries must be rebuilt; this is not a released-ABI compatibility layer. Boolean-only checks still propagate an
uncategorized/unspecified failure, while library-owned failures and application invariants use their owning enums.

Expected control states should not be represented as errors merely to terminate an operation. APIs such as iteration
should expose completion separately when that can be done without making the API materially worse. Cancellation may
remain an unsuccessful result when callers can identify it through a stable code.

## Consequences

Callers can branch on stable, platform-independent machine-readable errors without parsing or comparing strings.
Ordinary `SC_TRY` propagation remains concise and retains primary identity through plain `Result` boundaries, while
specialized APIs can expose stable backend detail and actionable context at an explicit size cost. Default English
strings and formatting code can be omitted, and alternate languages do not need to replace error identity.

Changing the representation is an ABI transition and all participating binaries must be rebuilt. Context is lost at
an intentional enriched-to-plain conversion, automatic nested cause chains are not provided, and category allocation
requires lightweight project-wide coordination. Library-specific result sizes and borrowed-data exceptions require
explicit review.

## Confirmation

Tests assert the size, alignment, trivial copyability, standard layout, success encoding, category/error round trips,
foreign-category conversion, enriched-result conversion, detail preservation, and `SC_TRY`/coroutine
propagation. Cross-platform tests assert equivalent backend failures have the same primary error and may assert their
different detail values. CI checks built-in category uniqueness and reserved ranges. Binary tests verify the omission
guarantees selected for executables, static libraries, shared libraries, and single-file artifacts by the formatter
packaging experiment. Single-file libraries compile both with and without optional diagnostics, and the complete suite
passes while the bridge is present and after its removal.

## Related

- [COMMON-0006 - Treat Common public layouts as cross-library API surface](common-0006-treat-common-public-layouts-as-cross-library-api-surface.md)
- [COMMON-0007 - Keep IGrowableBuffer as the minimal output growth adapter](common-0007-keep-igrowablebuffer-as-the-minimal-output-growth-adapter.md)
- [SC-0006 - Use explicit result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
- [SC-0014 - Use automated checks to protect architecture](../Global/sc-0014-use-automated-checks-to-protect-architecture.md)
