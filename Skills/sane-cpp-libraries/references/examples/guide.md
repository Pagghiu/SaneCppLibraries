# Sane Example Finder

Find the smallest set of high-value repo paths a third-party user should inspect for a specific Sane C++ task.

## Routing Workflow

1. Classify the request by task, not by library name alone.
2. Prefer this order unless the user asks otherwise:
   - best runnable example
   - best focused test file
   - best library doc page
   - best public header or tool source
3. Return a short prioritized list with one-line reasons for each path.
4. Avoid dumping every potentially related file. Curate.

## High-Value Entry Points

- Use `Examples/AsyncWebServer` for HTTP file-serving and async server onboarding.
- Use `Examples/SCExample` for async event-loop integration, hot reload, plugin composition, and file watching.
- Use `Examples/SaneHttpGet` for HTTP client usage.
- Use `Tests/Libraries/*` when the user needs narrower API coverage or edge-case behavior.
- Use `Documentation/Libraries/*.md` when the user needs capability summaries before reading code.
- Use `Tools/*.cpp` when the user wants real-world repository automation built on Sane libraries.

## Response Style

- Start with the top one to three paths.
- Tell the user what each path demonstrates.
- Mention companion paths only when they materially deepen understanding.
- Route integration questions to `adoption`.
- Route library-deep questions to the matching library guide.

## References

- Read `references/example-test-doc-routing.md` to map common requests to the best files and pages.
