@page library_threading Threading

@brief 🟩 Atomic, thread, pool, mutex, semaphore, barrier and more

[TOC]

[SaneCppThreading.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppThreading.h) is a library defining basic primitives for user-space threading and synchronization.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Threading.svg)


# Features
| Class                 | Description                       |
|:----------------------|:----------------------------------|
| SC::Thread            | @copybrief SC::Thread             |
| SC::ThreadPool        | @copybrief SC::ThreadPool         |
| SC::Mutex             | @copybrief SC::Mutex              |
| SC::RWLock            | @copybrief SC::RWLock             |
| SC::Barrier           | @copybrief SC::Barrier            |
| SC::Semaphore         | @copybrief SC::Semaphore          |
| SC::ConditionVariable | @copybrief SC::ConditionVariable  |
| SC::Atomic            | @copybrief SC::Atomic             |
| SC::EventObject       | @copybrief SC::EventObject        |

# Status
🟩 Usable  
All the main threading primitives are there.  
The Atomic header is really only being implemented for a few data types and needs some love to extend and improve it.

# Blog

Some relevant blog posts are:

- [August 2025 Update](https://pagghiu.github.io/site/blog/2025-08-31-SaneCppLibrariesUpdate.html)


# Videos

This is the list of videos that have been recorded showing some of the internal thoughts that have been going into this library:

- [Ep.13 - Simple ThreadPool](https://www.youtube.com/watch?v=e48ruImESxI)

## SC::Thread
@copydoc SC::Thread

## SC::ThreadPool
@copydoc SC::ThreadPool

## SC::Mutex
@copydoc SC::Mutex

## SC::EventObject
@copydoc SC::EventObject

## SC::Atomic
@copydoc SC::Atomic

## SC::Semaphore
@copydoc SC::Semaphore

## SC::RWLock
@copydoc SC::RWLock

## SC::Barrier
@copydoc SC::Barrier

## SC::ConditionVariable
@copydoc SC::ConditionVariable

# Roadmap

🟦 Complete Features:
- Support more types in Atomic<T>

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Threading`.
Single File counts
`SaneCppThreading.h`.
Standalone counts `SaneCppThreadingStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 433		| 926		| 1359	|
| Single File | 1088		| 1102		| 2190	|
| Standalone  | 1088		| 1102		| 2190	|
