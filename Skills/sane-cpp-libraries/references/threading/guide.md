# Sane Threading

## Quick Start

- Use this guide when the user needs synchronization or worker-thread guidance.
- Start with [references/threading-primitives.md](references/threading-primitives.md).

## Core Workflow

1. Inspect `Libraries/Threading/Threading.h`, `Atomic.h`, and `ThreadPool.h`.
2. Use the lightest primitive that solves the coordination problem.
3. Mention ownership and wakeup behavior when the user asks about concurrency design.
4. Route async event-loop questions to `async` when the work is driven by I/O rather than pure threads.

## What To Emphasize

- `Atomic` covers low-level atomic operations.
- `ThreadPool` helps when work should be distributed rather than manually spawned.
- Mutexes, semaphores, barriers, rw-locks, and condition variables each solve different coordination shapes.

## Pitfalls

- Do not suggest a heavier primitive when a lighter one works.
- Do not conflate plain threading with the async event loop.
- Do not omit shutdown or wakeup behavior when explaining synchronization.

## References

- [references/threading-primitives.md](references/threading-primitives.md)
- `Libraries/Threading/Threading.h`
- `Libraries/Threading/Atomic.h`
- `Libraries/Threading/ThreadPool.h`
