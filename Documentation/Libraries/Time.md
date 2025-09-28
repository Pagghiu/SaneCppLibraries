@page library_time Time

@brief 🟨 Time handling (relative, absolute, high resolution)

[TOC]

[SaneCppTime.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppTime.h) contains classes to measure time and compute or measure time intervals.

# Dependencies
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 127			| 136		| 263	|
| Sources   | 219			| 37		| 256	|
| Sum       | 346			| 173		| 519	|

# Features

| Class                             | Description                                   |
|:----------------------------------|:----------------------------------------------|
| SC::Time::Absolute                | @copybrief SC::Time::Absolute                 |
| SC::Time::Monotonic               | @copybrief SC::Time::Monotonic                |
| SC::Time::Realtime                | @copybrief SC::Time::Realtime                 |
| SC::Time::Relative                | @copybrief SC::Time::Relative                 |
| SC::Time::HighResolutionCounter   | @copybrief SC::Time::HighResolutionCounter    |

# Status
🟨 MVP  
This library is in MVP state but it doesn't have a clear roadmap.

# Description

## SC::Time::Absolute
@copydoc SC::Time::Absolute

### SC::Time::Absolute::parseLocal
@copydoc SC::Time::Absolute::parseLocal

## SC::Time::Monotonic
@copydoc SC::Time::Monotonic

### SC::Time::Monotonic::now
@copydoc SC::Time::Monotonic::now

## SC::Time::Realtime
@copydoc SC::Time::Realtime

### SC::Time::Realtime::now
@copydoc SC::Time::Realtime::now

## SC::Time::Relative
@copydoc SC::Time::Relative

## SC::Time::HighResolutionCounter
@copydoc SC::Time::HighResolutionCounter

### SC::Time::HighResolutionCounter::snap
@copydoc SC::Time::HighResolutionCounter::snap

### SC::Time::HighResolutionCounter::subtractApproximate
@copydoc SC::Time::HighResolutionCounter::subtractApproximate

### SC::Time::HighResolutionCounter::isLaterThanOrEqualTo
@copydoc SC::Time::HighResolutionCounter::isLaterThanOrEqualTo

# Blog

Some relevant blog posts are:

- [January 2025 Update](https://pagghiu.github.io/site/blog/2025-01-31-SaneCppLibrariesUpdate.html)

# Roadmap

🟩 Usable
- No Plan

🟦 Complete Features:
- No Plan

💡 Unplanned Features:
- No Plan
