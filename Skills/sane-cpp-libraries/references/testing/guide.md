# Sane Testing

## Quick Start

- Use this guide when the user asks about the Sane test framework or where to find examples of library behavior.
- Start with [references/test-layout-and-invocation.md](references/test-layout-and-invocation.md).

## Core Workflow

1. Inspect `Libraries/Testing/Testing.h` and `Testing.cpp` for the framework shape.
2. Read `Tests/SCTest` and `Tests/Libraries/*` to see how the framework is used in practice.
3. Use `TestCase`, `TestReport`, and the section helpers when explaining how a test is structured.
4. Route library-specific behavior questions back to the matching library guide after using tests as examples.

## What To Emphasize

- Tests are an example surface for Sane library usage.
- Sections help break one test file into several focused scenarios.
- The repository’s tests are the best place to learn expected behavior for each library.

## Pitfalls

- Do not present the framework as a generic external test runner.
- Do not hide the role of tests as documentation.
- Do not duplicate the library-specific guidance that belongs in the other library guides.

## References

- [references/test-layout-and-invocation.md](references/test-layout-and-invocation.md)
- `Libraries/Testing/Testing.h`
- `Tests/SCTest/*`
- `Tests/Libraries/*`
