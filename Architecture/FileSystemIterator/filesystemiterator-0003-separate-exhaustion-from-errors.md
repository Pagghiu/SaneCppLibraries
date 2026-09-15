# FILESYSTEMITERATOR-0003 - Separate Exhaustion From Errors

## Status

Accepted

## Context

`FileSystemIterator::enumerateNext()` historically returned an invalid `Result` both when traversal finished normally
and when an operation failed. The implementation recognized normal completion by comparing the returned English error
string with `"Iteration Finished"`, while callers had to invoke `checkErrors()` after the loop to discover whether a
false loop condition meant exhaustion or failure.

Normal completion is control flow, not an error. Giving it an error identity would keep the ambiguity and would make
localized or optional diagnostics part of program logic. At the same time, changing every compact
`while (iterator.enumerateNext())` loop to manage an output status would add substantial client churn.

Traversal errors can carry useful fixed-size context. In particular, native platform error values, the affected
directory depth, and the backend stage can distinguish failures without allocating or retaining path strings.

## Decision

`enumerateNext()` returns `bool`: `true` means that a new entry is available and `false` means either normal exhaustion
or a retained traversal failure. `checkErrors()` remains the explicit, repeatable way to distinguish those cases.
Normal exhaustion leaves the retained result successful and has no error code or diagnostic string.

The implementation uses a result-returning internal operation with a separate `hasEntry` output. This keeps ordinary
error propagation inside the platform backends without overloading `Result` validity with iteration state.

The iterator retains its first actual failure. After a failure, further advancement returns `false` without accessing
native traversal state. `init()` starts a new traversal and resets the retained result; a failed initialization is
itself retained. Manual recursion failures are retained by the public wrapper. Early loop termination remains valid,
and `checkErrors()` reports only failures encountered so far.

FileSystemIterator owns its error category, primary error enum, backend/API-stage detail enum, enriched result, and
optional English formatter. The enriched result contains only the plain `Result`, a native error number, the affected
directory depth, and the backend/API stage. Depth and detail are 16-bit fixed-width values: successful path growth is
bounded by `StringPath::MaxPath`, which is currently at most 4096 and statically constrained to fit in `uint16_t`. This
keeps the allocation-free, standard-layout, trivially copyable result at 16 bytes after the legacy Result message
pointer is removed.
Conversion to plain `Result` preserves category and error identity while deliberately discarding the additional context.

On current 64-bit targets the temporary 16-byte compatibility bridge makes the enriched result 24 bytes. Removing the
legacy pointer returns plain `Result` to eight bytes and this enriched type to the 16-byte target. The temporary size is
not a stable ABI entitlement.

## Consequences

Existing loop conditions remain source-compatible. Callers that stored or propagated the direct return value of
`enumerateNext()` must migrate to the boolean completion contract and use `checkErrors()` for traversal failures.
Using `SC_TRY(iterator.enumerateNext())` is no longer meaningful because normal exhaustion is a false boolean.

Every complete traversal loop must check `checkErrors()` before treating absence of another entry as success. The
library documentation and in-repository callers must demonstrate this pattern.

Canonical English messages remain opt-in and outside the mandatory public header. Program logic depends only on the
library-owned numeric identity and fixed-size context.

Native end-of-directory detection, resource ownership, and recursion mechanics remain separate behavioral concerns.
Changes to those paths require focused regression tests and may be committed independently from this API migration.
