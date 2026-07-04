# Threading Architecture

## Purpose

Threading is the dependency-free native synchronization and thread-execution primitive library. It must provide the basic building blocks other libraries and applications can use without adopting STL threading, allocation-backed task queues, or higher-level execution frameworks.

## Architectural Shape

The public interface includes `Thread`, `Mutex`, `ConditionVariable`, `EventObject`, `RWLock`, `Barrier`, `Semaphore`, `ThreadPool`, and narrow `Atomic` specializations. Native synchronization storage is represented with opaque inline storage in public types and platform-specific implementations in internal files.

Thread and ThreadPool lifetimes must remain explicit. Started threads are joined or detached. ThreadPool tasks are caller-owned nodes with stable addresses.

## Boundaries

Threading owns local user-space synchronization primitives, native thread start/join/detach, thread sleep, a simple fixed-worker pool, and minimal atomic wrappers. It does not own async event loops, coroutine scheduling, futures/promises, work-stealing schedulers, or allocation-backed task ownership.

## Similarities With Other Libraries

Threading follows the same platform-abstraction pattern as Socket and Time: dependency-free public surface, native implementation hidden from headers, and tests through the public interface. Like Process and Plugin, it makes lifecycle responsibilities explicit instead of hiding them in destructors or background managers.

## Differences From Other Libraries

Unlike Async or Await, Threading is not an orchestration runtime. Unlike Containers or Memory, it does not own dynamic task storage. Unlike Time, it can block threads through sleep and synchronization but does not own clock semantics beyond sleeping.

## Inspirations

The evidenced inspirations are native OS threading primitives and classic synchronization concepts: mutexes, condition variables, events, semaphores, barriers, read-write locks, atomics, and a fixed worker thread pool.

## Anti-Inspirations

Inferred anti-inspirations include `std::thread`/`std::atomic` as mandatory dependencies, implicit join-on-destroy behavior, detached-by-default background work, and heap-backed task queues that obscure task lifetime.

## Architectural Choices

- Keep Threading dependency-free.
- Keep platform primitives hidden behind opaque public storage.
- Require explicit thread join or detach.
- Require stable caller-owned ThreadPool task storage.
- Keep `Atomic` conservative until broader type support is deliberately designed.

## Explicitly Excluded Targets

- Futures, promises, executors, or coroutine scheduling.
- Work-stealing or dynamically allocating task systems.
- Hidden background lifecycle management.
- Requiring the C++ standard threading library.
- Becoming the Async or Await runtime.

## Sources

- [Threading documentation](../../Documentation/Libraries/Threading.md)
- [Threading public API](../../Libraries/Threading/Threading.h)
- [ThreadPool public API](../../Libraries/Threading/ThreadPool.h)
- [Atomic public API](../../Libraries/Threading/Atomic.h)
- [Threading tests](../../Tests/Libraries/Threading/ThreadingTest.cpp)
- [ThreadPool tests](../../Tests/Libraries/Threading/ThreadPoolTest.cpp)
- [THREADING-0001 - Keep Threading as a dependency-free native primitive layer](threading-0001-keep-threading-as-a-dependency-free-native-primitive-layer.md)
- [THREADING-0002 - Require explicit Thread and ThreadPool lifetimes](threading-0002-require-explicit-thread-and-threadpool-lifetimes.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)

## Decision Log

- [THREADING-0001 - Keep Threading as a dependency-free native primitive layer](threading-0001-keep-threading-as-a-dependency-free-native-primitive-layer.md)
- [THREADING-0002 - Require explicit Thread and ThreadPool lifetimes](threading-0002-require-explicit-thread-and-threadpool-lifetimes.md)
