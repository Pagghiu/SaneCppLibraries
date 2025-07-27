@page library_threading Threading

@brief 🟥 Atomic, thread, thread pool, mutex, condition variable

[TOC]

Threading is a library defining basic primitives for user-space threading and synchronization.

# Dependencies
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

# Statistics
- Lines of code (excluding comments): 895
- Lines of code (including comments): 1178

# Features
| Class                 | Description                       |
|:----------------------|:----------------------------------|
| SC::Thread            | @copybrief SC::Thread             |
| SC::ThreadPool        | @copybrief SC::ThreadPool         |
| SC::Mutex             | @copybrief SC::Mutex              |
| SC::ConditionVariable | @copybrief SC::ConditionVariable  |
| SC::Atomic            | @copybrief SC::Atomic             |
| SC::EventObject       | @copybrief SC::EventObject        |

# Status
🟥 Draft  
Only the features needed for other libraries have been implemented so far.
The Atomic header is really only being implemented for a few data types and needs some love to extend and improve it.

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

# Roadmap
🟨 MVP
- Scoped Lock / Unlock

🟩 Usable
- Semaphores

🟦 Complete Features:
- Support more types in Atomic<T>
- ReadWrite Lock
- Barrier
