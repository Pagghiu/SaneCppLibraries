---
name: sane-cpp-libraries
description: Use when adopting, integrating, debugging, or composing Sane C++ Libraries APIs. Covers selecting a Sane library, single-file or repo integration, and correct use of Foundation, Memory, Containers, Async, File, Process, networking, serialization, plugins, build tools, and tests. Do not use for general C++ design that does not require Sane APIs; use Sane C++ Style for that.
---

# Sane C++ Libraries

Help users compose the APIs that exist in this checkout, rather than translating them into STL-shaped abstractions or guessing overloads. This is an adoption skill, not a requirement to adopt all Sane libraries.

## Start Here

- For integration or library selection, read [getting started](references/getting-started.md), then [adoption](references/adoption/guide.md) if needed.
- For one library, read only its matching `references/<topic>/guide.md`; use [the topic map](references/topic-map.md) for ambiguous requests.
- For an async process with pipe capture, read [the composition recipe](references/async-process-composition.md) as well as `async`, `file`, and `process`.
- Before emitting code, verify the selected public header and the cited test or example in the local checkout. The source locations behind the composition guidance are in [source anchors](references/source-anchors.md).

## Composition rules that prevent real mistakes

- Treat a `Result` as an obligation: check it and propagate or classify it at the boundary. Do not infer successful I/O from EOF or ignore setup failures.
- Storage, descriptors, request objects, callback captures, and input views must outlive each active operation. The event loop does not own or move `AsyncRequest` objects.
- Define the completion predicate before writing callbacks. For a process with stdout and stderr capture, process exit alone is insufficient: wait for process completion and EOF/error handling on each stream before reuse or destruction.
- A recurring `AsyncFileRead` is reactivated only after its result is handled. Continue reading after a bounded capture buffer fills when draining is required; retain a prefix separately from the byte total.
- `PipeOptions::blocking` applies to pipe creation, not just the parent read end. Check the platform behavior and child contract before using a pipe as child stdout/stderr. Make inheritance deliberate: an extra inherited writer can suppress EOF.
- Pass the descriptors expected by `Process::launch`; inspect the overload and the process tests rather than constructing its nested redirection types from guessed handles.
- For a capacity-bounded scheduler, make slots—not merely counters—reusable. Reset a slot only after its request/descriptors and completion predicate make reuse safe.

## Integration and verification

Use the smallest distribution route that meets the request: a single-file library for a narrow dependency footprint, or `SC.cpp` plus public library headers for multi-library work. Follow the selected guide for platform link requirements; do not include `Internal` or test headers in an application.

When changing Sane code, follow the repository's `AGENTS.md`, build the affected target before running it, run focused tests, and preserve independence checks where a new dependency could be introduced. When answering without changing code, say which header/test establishes any non-obvious claim.

## Topic Guides

### Onboarding And Navigation

- Adoption and integration: [references/adoption/guide.md](references/adoption/guide.md)
- Best examples, tests, docs, and source entry points: [references/examples/guide.md](references/examples/guide.md)
- SC.sh, SC.bat, and custom tool invocation: [references/tools/guide.md](references/tools/guide.md)
- SC::Build project setup: [references/build/guide.md](references/build/guide.md)
- Test layout and SCTest usage: [references/testing/guide.md](references/testing/guide.md)

### Core Types And Data Structures

- Global rules and adaptation patterns: [references/core-patterns/guide.md](references/core-patterns/guide.md)
- Foundation primitives such as `Result`, `Span`, and `Function`: [references/foundation/guide.md](references/foundation/guide.md)
- Buffers, allocators, and owned storage: [references/memory/guide.md](references/memory/guide.md)
- Container choice and capacity behavior: [references/containers/guide.md](references/containers/guide.md)
- String formatting, conversion, and path helpers: [references/strings/guide.md](references/strings/guide.md)
- Time types and clock selection: [references/time/guide.md](references/time/guide.md)
- Threads and synchronization primitives: [references/threading/guide.md](references/threading/guide.md)

### I/O, Async, And Platforms

- Event loop, requests, and wake-up integration: [references/async/guide.md](references/async/guide.md)
- Experimental coroutine wrapper over Async: [references/await/guide.md](references/await/guide.md)
- Backpressure-aware stream pipelines: [references/async-streams/guide.md](references/async-streams/guide.md)
- Cross-library async composition recipes: [references/async-networking/guide.md](references/async-networking/guide.md)
- Raw synchronous sockets and DNS: [references/socket/guide.md](references/socket/guide.md)
- HTTP server, parser, and file server flows: [references/http/guide.md](references/http/guide.md)
- Native-backend streaming HTTP client flows: [references/http-client/guide.md](references/http-client/guide.md)
- Descriptor-based file and pipe I/O: [references/file/guide.md](references/file/guide.md)
- Path-level filesystem operations: [references/filesystem/guide.md](references/filesystem/guide.md)
- Directory traversal: [references/filesystem-iterator/guide.md](references/filesystem-iterator/guide.md)
- File and folder change watching: [references/filesystem-watcher/guide.md](references/filesystem-watcher/guide.md)
- Child process launch and redirection: [references/process/guide.md](references/process/guide.md)
- Serial port configuration and opening: [references/serial-port/guide.md](references/serial-port/guide.md)

### Reflection, Serialization, And Plugins

- Reflection metadata and shape design: [references/reflection/guide.md](references/reflection/guide.md)
- Reflection adapters for Sane containers: [references/containers-reflection/guide.md](references/containers-reflection/guide.md)
- End-to-end serialization choice and composition: [references/serialization/guide.md](references/serialization/guide.md)
- Binary serialization flows: [references/serialization-binary/guide.md](references/serialization-binary/guide.md)
- Text and JSON serialization flows: [references/serialization-text/guide.md](references/serialization-text/guide.md)
- Hashing algorithms and workflows: [references/hashing/guide.md](references/hashing/guide.md)
- Runtime-loaded plugins and host exports: [references/plugin/guide.md](references/plugin/guide.md)
- Plugin plus build-system integration recipes: [references/plugin-build/guide.md](references/plugin-build/guide.md)
