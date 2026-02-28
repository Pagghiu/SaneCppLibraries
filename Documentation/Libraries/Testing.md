@page library_testing Testing

@brief 🟨 Simple testing framework used by all of the other libraries

[TOC]

[SaneCppTesting.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppTesting.h) is a very simple test framework that allows splitting tests in sections and record successful/failed expectations.

# Dependencies
- Dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

![Dependency Graph](Testing.svg)


# Features
| Class             | Description               |
|:------------------|:--------------------------|
| SC::TestCase      | @copybrief SC::TestCase   |      
| SC::TestReport    | @copybrief SC::TestReport |

# Status

🟨 MVP  
Testing library is minimal but it's useful as is for now.

# Description
Testing integrates with the SC::Result object that is part of [Foundation](@ref library_foundation) library.
So far it doesn't support test discovery and all tests must be manually invoked in the main test file.

## SC::TestCase
@copydoc SC::TestCase

## SC::TestReport
@copydoc SC::TestReport

# Roadmap

🟩 Usable
- Test discovery

🟦 Complete Features:
- IDE Integration

💡 Unplanned Features:
- Template tests

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 133			| 102		| 235	|
| Sources   | 295			| 28		| 323	|
| Sum       | 428			| 130		| 558	|
