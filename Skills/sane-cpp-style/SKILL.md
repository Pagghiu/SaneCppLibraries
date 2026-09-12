---
name: sane-cpp-style
description: Design, implement, or review C++ libraries and systems code in Sane's explicit-resource architectural style. Use for new library designs, bounded services, async/stateful components, or existing-code changes where ownership, storage, capacity, cleanup, errors, header boundaries, or dependencies matter. Do not require Sane APIs or the full Sane framework; do not use for ordinary naming-only or formatting-only requests.
---

# Sane C++ Style

Use the principles as design tools, not a mandate to retrofit every program into Sane C++ Libraries. Preserve the host project's explicit constraints; choose a policy that makes resources, failure, and lifetime understandable at the API boundary.

## Start with the resource model

Before selecting classes or names, state (in a design note, code comments, or review) the answer to these questions:

1. Which resources exist—memory, handles, requests, threads, buffers, locks, callbacks—and who owns each?
2. What borrows what, and for how long? Which addresses or identities must stay stable while work is active?
3. What bounds concurrency, queued work, output, recursion, retries, and shutdown time? What happens at each limit?
4. Which operations can fail, including partial setup and cleanup? How is the failure surfaced, and what remains live afterward?
5. What condition makes an operation complete, cancellable, reusable, or destructible?

Read [resource and lifetime design](references/resource-lifetime.md) for new components or stateful/asynchronous code. Read [API and dependency boundaries](references/api-boundaries.md) when shaping a public library or changing dependencies. Read [review and verification](references/review-and-verification.md) before finishing an implementation or review.

## Defaults, not slogans

Prefer caller-owned storage or an explicit allocation policy when a reusable component needs variable-sized state. Prefer views/spans for borrowed input, and make ownership transfer visible in type or API shape. Group things that begin, end, and are reused together into one owner/slot rather than scattering their state across unrelated maps or callbacks.

Use bounded capacity when predictable resource use matters. Return or report exhaustion explicitly; never silently discard work unless lossy behavior is an intentional, named policy. Make backpressure/cancellation choices visible: reject, wait, queue to a stated limit, coalesce, drain, or stop.

Check every fallible boundary. A partial initialization path needs an owner and cleanup plan before the next fallible step. Do not redefine a read error as EOF, cancellation as success, or a child exit as full pipeline completion without an explicit contract.

## Library-facing defaults

For portable, independently consumable libraries, keep public headers light: avoid system headers and avoid implementation-heavy code or templates when a `.cpp` boundary works. Keep dependencies intentional and minimal; a convenient sibling dependency is a design decision, not a reflex. Prefer simple concrete APIs over hidden global state, implicit allocation, shared ownership, or framework capture.

These are strong Sane defaults, not universal laws. An allocator, container, GUI application, or platform adapter may legitimately allocate, own a global runtime, use system headers internally, or depend on another library. Make the exception explicit in the API/design and test its failure and shutdown behavior. Do not reject a design merely for using `std`, RAII, exceptions, templates, or allocation when the host project intentionally permits them; focus on whether the resource policy is clear and testable.

## Keep style separate from architecture

First make the resource and error model correct. Then apply local naming and formatting conventions. In Sane library code specifically, honor its repository rules (including no STL/exceptions/RTTI, result propagation, caller-provided storage, and public-header hygiene); these are project constraints, not prerequisites for applying this skill elsewhere.

## Finish with fast evidence

Write behavior tests before relying on source-pattern checks. Exercise the normal path, capacity exhaustion, partial setup, cancellation/shutdown, and reuse where applicable. Compile with warnings enabled; use sanitizers and focused tests when they are available. Review source for ownership/cleanup/dependency obligations that execution cannot inject. Record whether a finding is a test defect, library defect, design omission, or implementation defect.
