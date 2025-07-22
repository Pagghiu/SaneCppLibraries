@page library_testing Testing

@brief 🟨 Simple testing framework used by all of the other libraries

[TOC]

Testing is a very simple test framework that allows splitting tests in sections and record successful/failed expectations.

# Dependencies
- Direct dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Strings](@ref library_strings)
- All dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Strings](@ref library_strings)

# Statistics
- Lines of code (excluding comments): 303
- Lines of code (including comments): 381

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
