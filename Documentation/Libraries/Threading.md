@page library_threading Threading

@brief 🟩 Atomic, thread, thread pool, mutex, semaphore, barrier, rw-lock, condition variable

[TOC]

[SaneCppThreading.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppThreading.h) is a library defining basic primitives for user-space threading and synchronization.

# Dependencies
- Dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

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
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 399			| 248		| 647	|
| Sources   | 906			| 167		| 1073	|
| Sum       | 1305			| 415		| 1720	|
