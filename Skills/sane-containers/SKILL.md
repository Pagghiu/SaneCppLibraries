---
name: sane-containers
description: Sane C++ container selection and usage for Vector, SmallVector, Array, VectorMap, VectorSet, and ArenaMap. Use when deciding which container to adopt, how capacity affects behavior, or how Containers depends on Memory.
---

# Sane Containers

## Overview

Use this skill when the user needs to choose or understand a Sane container type. Focus on capacity, allocation behavior, and the relationship between containers and `Memory`.

## Use This Skill When

- A request asks whether to use `Vector`, `SmallVector`, or `Array`.
- A request asks how to keep container operations allocation-aware.
- A request needs the map/set variants provided by the library.
- A request needs a container that plays well with user-provided allocators.

## Start Here

- Read [references/container-selection.md](references/container-selection.md).
- Inspect `Libraries/Containers/Vector.h`, `Array.h`, `VectorMap.h`, `VectorSet.h`, and `ArenaMap.h`. `SmallVector` is defined in `Vector.h`.
- Use `Tests/Libraries/Containers/VectorTest.cpp`, `ArrayTest.cpp`, `SmallVectorTest.cpp`, `VectorMapTest.cpp`, `VectorSetTest.cpp`, and `ArenaMapTest.cpp`.

## Key Guidance

- Prefer `Array` when heap allocation must never happen.
- Prefer `SmallVector` when a good inline default can avoid temporary heap use.
- Prefer `Vector` when the caller needs a general-purpose dynamic container.
- Prefer `VectorMap` or `VectorSet` when a compact ordered map or set fits the problem.
- Treat `ArenaMap` as the specialized arena-backed option.

## Pitfalls

- Do not confuse the capacity guarantees of `Array` and `SmallVector`.
- Do not pretend containers are independent from `Memory`.
- Do not recommend `std::` containers as the primary Sane answer unless the user explicitly asks.
- Do not bury the allocation story when the choice affects performance or correctness.
