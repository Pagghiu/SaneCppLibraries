# TOOLS-0024 - Validate Full Pinned Git Revisions

Status: Accepted

## Context

Shallow Git package recipes fetch a full object ID and can use a short version label in receipts
and directory names. Validating an installed checkout against the label alone can accept a
different commit with the same prefix.

## Decision

When a recipe specifies a shallow revision, compare the installed HEAD with the full pin.
Retain prefix matching for recipes that specify only a version. Use the same revision check
for an existing installation and a freshly fetched checkout.

When the package tool is built with Fil-C, clear `LD_LIBRARY_PATH` for native Git child
processes so the native loader resolves Git's own dependencies. The package tool keeps its
Fil-C library path for its own runtime dependencies.

## Confirmation

In SupportToolsTest, verify a local checkout against its actual pin and reject a different
full-length pin that shares its version prefix.
