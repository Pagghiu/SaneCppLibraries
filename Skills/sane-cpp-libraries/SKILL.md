---
name: sane-cpp-libraries
description: Unified guidance for Sane C++ Libraries. Use when the user is adopting Sane C++ Libraries, choosing the right Sane library, looking for examples or tests, or working with Foundation, Memory, Strings, Containers, Async, Await, Socket, Http, File, FileSystem, Process, Time, Threading, Reflection, Serialization, Plugin, Build, Tools, or Testing.
---

# Sane C++ Libraries

Use this as the single installed skill for every Sane C++ Libraries request.

## Start Here

- Read [references/getting-started.md](references/getting-started.md) first.
- Then read the smallest matching topic guide under `references/*/guide.md`.
- Use [references/topic-map.md](references/topic-map.md) when the request is broad, ambiguous, or spans multiple libraries.

## Discovery Workflow

1. Ground the answer in the repo-wide rules from `getting-started`.
2. Pick one primary topic guide and at most one or two companion guides.
3. Inspect the linked public headers, tests, examples, docs, or tools before answering.
4. Prefer repo-specific guidance over generic C++ advice.

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
- Draft coroutine wrapper over Async: [references/await/guide.md](references/await/guide.md)
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
