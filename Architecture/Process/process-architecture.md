# Process Architecture

## Purpose

Process is the synchronous child-process and process-snapshot library. It must make process launch, standard stream routing, environment construction, working directory setup, process chaining, and fork-style snapshotting available without hidden allocation or shell-dependent behavior.

## Architectural Shape

The public interface is intentionally small: `Process`, `ProcessChain`, `ProcessEnvironment`, and `ProcessFork`. `Process` owns launch formatting and stdio configuration. `ProcessChain` composes caller-owned `Process` objects with pipes. `ProcessEnvironment` reads the current process environment. `ProcessFork` exposes the narrow fork/snapshot workflow.

Command arguments and environment values must be formatted into explicit native arenas. The inline storage is the default convenience path; larger callers pass spans. Standard streams must be modeled as typed choices over spans, growable buffers, file descriptors, pipes, inherit, or ignore behavior.

## Boundaries

Process owns child process creation and synchronous waiting. It may depend on File because process I/O is descriptor and pipe oriented. It must not become a shell parser, task scheduler, async process manager, terminal emulator, container/sandbox runtime, or string/container ownership library.

Platform-specific launch and fork details belong in Process internal implementation files. Public headers should continue to expose portable Sane types and explicit storage choices.

## Similarities With Other Libraries

Process follows the same low-level platform-wrapper pattern as File, Socket, SerialPort, and Threading: public Sane types, native handles behind small abstractions, explicit `Result` failures, and OS-specific implementation hidden from public headers.

It shares the same dependency-cleanup goals as Socket and Plugin: use Common primitives and small local helpers rather than reaching for higher-level Sane libraries.

## Differences From Other Libraries

Unlike Socket and Time, Process intentionally depends on File because pipes and descriptors are part of its core interface. Unlike Plugin, Process launches external programs instead of loading code into the current process. Unlike Async, Process does not own event-loop process monitoring.

`ProcessFork` is deliberately more caveated than ordinary `Process` launch because it clones the current process state and inherits platform fork hazards.

## Inspirations

`ProcessChain` is inspired by POSIX shell pipelines, as documented in the public comments, but must express that model through typed `Process` objects and `PipeDescriptor` handoffs rather than shell strings.

`ProcessFork` is inspired by native process cloning mechanisms such as POSIX `fork` and the Windows clone path used by the implementation. Its supported shape is the common-denominator snapshot workflow, not every behavior those native APIs permit.

## Anti-Inspirations

Inferred anti-inspirations: shell command concatenation, implicit environment allocation, automatic background task frameworks, and broad fork-after-threading models. These designs hide quoting, storage, handle inheritance, or runtime safety details that Process is meant to surface.

## Architectural Choices

- Keep command and environment data in explicit arenas.
- Keep stdio redirection typed and descriptor-aware.
- Keep process chains caller-owned and pipe-based.
- Keep fork-style behavior narrow, documented, and skippable where platform support is unsafe.
- Keep failure propagation explicit with `Result`.

## Explicitly Excluded Targets

- Parsing shell syntax or emulating shell process substitution.
- Owning async process lifecycle APIs.
- Providing a sandbox or security boundary.
- Allocating hidden command/environment buffers.
- Making `ProcessFork` a general portable multitasking primitive.

## Sources

- [Process documentation](../../Documentation/Libraries/Process.md)
- [Process public API](../../Libraries/Process/Process.h)
- [Process implementation](../../Libraries/Process/Process.cpp)
- [Process tests](../../Tests/Libraries/Process/ProcessTest.cpp)
- [PROCESS-0001 - Keep Process arguments and environment in explicit arenas](process-0001-keep-process-arguments-and-environment-in-explicit-arenas.md)
- [PROCESS-0002 - Model Process I/O and chains as explicit File/Pipe handoffs](process-0002-model-process-io-and-chains-as-explicit-file-pipe-handoffs.md)
- [PROCESS-0003 - Keep ProcessFork as a caveated snapshot primitive](process-0003-keep-processfork-as-a-caveated-snapshot-primitive.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)

## Decision Log

- [PROCESS-0001 - Keep Process arguments and environment in explicit arenas](process-0001-keep-process-arguments-and-environment-in-explicit-arenas.md)
- [PROCESS-0002 - Model Process I/O and chains as explicit File/Pipe handoffs](process-0002-model-process-io-and-chains-as-explicit-file-pipe-handoffs.md)
- [PROCESS-0003 - Keep ProcessFork as a caveated snapshot primitive](process-0003-keep-processfork-as-a-caveated-snapshot-primitive.md)
