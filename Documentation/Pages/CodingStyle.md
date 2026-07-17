@page page_coding_style Coding Style

The style rules make ownership, failure, and platform behavior visible in ordinary C++ code. Consistency matters, but
the deeper goal is code that a reviewer or coding agent can change without guessing at hidden contracts.

[TOC]

# Start from the repository constraints

Library code does not use STL containers, exceptions, RTTI, or hidden allocation. Public headers do not include system
headers. APIs return errors and accept caller-owned storage. These constraints are architectural; formatting should make
them easier to see rather than disguise them.

Before editing a subsystem, read its neighboring headers, tests, and any local `AGENTS.md`. Prefer an existing type or
pattern over introducing a second vocabulary for the same operation.

# Let the formatter establish the baseline

The repository uses the pinned clang-format version installed by SC::Package:

```bash
./SC.sh format execute
```

CI checks the same format. Use `// clang-format off` only around a small structure whose manual alignment communicates a
real pattern, such as a dense reflection table. Turn formatting back on immediately afterward.

Formatting can create excessive horizontal padding when unrelated declarations are aligned together. Split fields or
methods into short semantic groups before formatting rather than accepting a wide block of whitespace.

# Name by role

- Use `CamelCase` for structs, namespaces, template parameters, and source file names.
- Use `camelCase` for functions, methods, variables, and fields.
- Use `SCREAMING_CASE` for preprocessor macros.
- Put public library types in `SC` or, for a cohesive subsystem, a nested namespace below `SC`.
- Keep `using namespace` inside function scope.

The `SC` namespace is intentionally flat in many libraries. Search before adding a public name so two independent
libraries do not collide when amalgamated into one translation unit.

# Make failure part of the type

Operations that can fail return `Result` or another checked value. Propagate failures with the `SC_TRY` family rather
than ignoring them:

```cpp
Result loadConfiguration(StringView path, String& output)
{
    File file;
    SC_TRY(file.open(path, File::OpenMode::Read));
    return file.readUntilEOF(output);
}
```

Value-returning functions should normally be `[[nodiscard]]`. If a deliberately best-effort operation must ignore a
result, keep the warning suppression narrow and explain why the result cannot affect correctness.

Do not use exceptions for control flow. A cleanup path must be visible on every return path, usually through
`MakeDeferred` or an explicit close operation.

# Keep outputs and ownership explicit

When a function already needs to return status, write its value through an output parameter:

- use a reference for required output;
- use a pointer for optional output;
- document when the output is written and whether it remains unchanged on failure.

Views such as `StringView` and `Span` borrow memory. Owning containers and storage objects must outlive every borrowed
view. APIs that may grow should accept an `IGrowableBuffer` or another explicit storage interface instead of allocating
internally.

Pointers mean “optional” unless a more specific contract says otherwise. References mean “required”. Avoid raw owning
pointers.

# Design public headers as boundaries

Public headers:

- include other public Sane C++ headers with repository-relative paths;
- avoid operating-system headers and third-party implementation headers;
- expose platform-neutral types;
- keep platform handles behind Sane C++ descriptors or private storage;
- end with a trailing newline.

Put operating-system includes and implementation details in `.cpp`, `.inl`, or `Internal` files. Isolate platform
branches in focused functions rather than scattering `#if` blocks through an algorithm.

Use `struct` for consistency with the existing codebase. Order a struct so a reader sees its public contract before
implementation details: types and constants, lifecycle, operations, then storage.

# Prefer small, testable operations

Functions should do one coherent job and return enough information for the caller to decide what happens next. Avoid
boolean parameters that silently switch between unrelated modes; use an options struct or distinct operation when the
behavior deserves a name.

Comments should explain a constraint, invariant, or non-obvious platform choice. Do not narrate syntax. Public API
documentation should state ownership, failure behavior, and important lifetime rules.

# Write tests as contract examples

Add or update the test in `Tests/Libraries/<Library>`. Name sections after behavior rather than implementation steps.
Cover the successful path, caller-storage exhaustion, invalid input, and relevant platform differences.

Tests often become the most precise usage examples in the repository. Keep setup explicit enough that a reader can see
which object owns each buffer, task, handle, and callback.

# Review checklist

Before committing, ask:

- Does every fallible result get checked or intentionally suppressed?
- Can the caller see who owns memory and how long borrowed views remain valid?
- Did a system or third-party header leak into a public header?
- Does the change create an unnecessary dependency between libraries?
- Are Debug, Release, single-file, and platform-sensitive paths covered as appropriate?
- Did formatting improve the structure, and did you inspect the resulting diff?

The contribution workflow and required validation are described in [CONTRIBUTING.md](https://github.com/Pagghiu/SaneCppLibraries/blob/main/CONTRIBUTING.md)
and [Building (Contributor)](@ref page_building_contributor).
