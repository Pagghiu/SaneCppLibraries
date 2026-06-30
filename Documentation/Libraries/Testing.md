@page library_testing Testing

@brief 🟨 Simple testing framework used by all of the other libraries

[TOC]

[SaneCppTesting.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppTesting.h) is a very simple test framework that allows splitting tests in sections and record successful/failed expectations.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

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
Testing integrates with the SC::Result object provided by the Common fragments.
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
LOC counts exclude comments. Library counts files physically under `Libraries/Testing`.
Single File counts
`SaneCppTesting.h`.
Standalone counts `SaneCppTestingStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 155		| 791		| 946	|
| Single File | 1178		| 1382		| 2560	|
| Standalone  | 1178		| 1382		| 2560	|
