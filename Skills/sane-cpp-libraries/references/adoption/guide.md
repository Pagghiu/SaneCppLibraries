# Sane Adoption Guide

Help a third-party user choose a practical adoption path for Sane C++ Libraries and get to the right integration steps quickly.

## Start Here

Identify the user's goal before giving instructions:

- If they want one or two libraries with minimal repo footprint, prefer the single-file path.
- If they want multiple libraries, examples, tests, or easier exploration of public headers, prefer repo integration through `SC.cpp`.
- If they mention plugins, also check export requirements and Linux `-rdynamic`.
- If they are mostly asking "which library should I use?", consult `references/library-selection-map.md`.

## Integration Workflow

### Single-file route

Use this when the user wants a lightweight adoption path or to vendor only one library.

1. Tell them to obtain the amalgamated header from the latest release or assemble it from the repo.
2. Tell them to include `SaneCpp<Library>.h` in headers.
3. Tell them to put `#define SANE_CPP_IMPLEMENTATION` before including that header in exactly one `.cpp` file.
4. Mention `SC_COMPILER_ENABLE_STD_CPP=1` only if they plan to keep using the standard library.
5. Check platform link requirements in `references/integration-modes-and-linking.md`.

### Full-repo route

Use this when the user needs several libraries, wants to inspect tests/examples, or is comfortable vendoring the repo.

1. Tell them to vendor the repo.
2. Tell them to add `SC.cpp` to their build.
3. Tell them to include only public headers under `Libraries/<Library>/*.h`.
4. Tell them not to include `Internal` or `Tests` headers from library folders.
5. Add platform link requirements and any plugin-specific host export notes.

## Response Style

- Prefer the smallest viable integration path first.
- Be explicit about platform-specific link requirements.
- Mention the project rules that matter externally: no STL by default, no exceptions, caller-owned memory.
- Route deeper API usage to the relevant library guide for that API surface.
- Route example hunting to `examples`.

## References

- Read `references/integration-modes-and-linking.md` for exact integration steps, platform notes, and plugin caveats.
- Read `references/library-selection-map.md` when the user is choosing libraries by task.
