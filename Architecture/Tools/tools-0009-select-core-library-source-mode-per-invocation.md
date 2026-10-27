# TOOLS-0009 - Select Core Library Source Mode Per Invocation

Status: Accepted
Date: 2026-10-26

## Context

Development benefits from separate source compilation and small incremental rebuilds. Bulk CI builds may
benefit from fewer compiler startups and less repeated header parsing. The existing addSaneCppLibraries API
already supports separate sources and the root SC.cpp unity source, but callers previously had to edit their
build description to compare those choices. Targets have different export, runtime, language and exception
contracts, so a universal shared object library cannot be assumed compatible.

## Decision

Add an optional Parameters::libraries override and expose it as --libraries default|separate|unity for
configure, compile, run and coverage actions. An unset override preserves each addSaneCppLibraries call's
existing mode. Repository tests continue to declare separate compilation by default. Explicitly selected
single-file packaging probes and other sources are unaffected by the helper's override.

Keep compiler/linker options, test sources and target contracts unchanged. Use the same mode when compiling
and running, and regenerate generated IDE projects when switching. Ensure SC.cpp includes all core .cpp
sources discovered by separate compilation, including Await and Hashing C bindings; experimental sources
remain explicitly selected outside the core helper. Prefix colliding private Await allocator helpers so the
C++20 core can form one translation unit. Treat SC.cpp as owning the exported definitions for Native/Make
symbol discovery; SC_EXPORT_LIBRARY_* visibility settings continue to select public library symbols.

## Consequences and validation

This provides an explicit cold-build experiment without introducing a cache dependency or changing normal
incremental development. A single unity source may increase optimized compile time by reducing parallelism;
it is not enabled in CI merely because it reduces translation-unit count. Local four-worker Release probes
showed separate core compilation around3.8s versus8.5s unity on Apple Clang and6.6s versus16.5s on Linux GCC.
Four smaller unity batches did not establish a wall-time improvement in those probes. These are core-only
samples, not hosted complete-build comparisons; MSVC and full linking must be assessed separately.

Verify CLI defaults/overrides/rejections, source inventory, switching back to separate mode, full Debug and
Release execution, and standalone single-file independence. Generated Make/Xcode/Visual Studio paths must
select the requested source form when regenerated. Compiler-specific CI adoption requires a measured net win
and retention of separate-source compilation coverage.

## Windows CI pilot

A clean generated MSVC Release SCBuildTest rebuild on the Windows VM took53.687 seconds with separate
core sources and40.828 seconds with unity (24% lower). Both retain /O2 /GL and include full LTCG/linking;
this is one paired VM sample, not a hosted guarantee. Pilot unity in the existing Windows Release build-fixture
jobs, passing the mode to both compile and run. Keep every fixture, compiler/OS lane and runtime argument,
and retain separate-source SCTest/core coverage. Record hosted compile timing and test results before
expanding unity to other targets or compilers. Posix jobs retain separate compilation based on their measurements.

The first hosted comparison is mixed: Windows2022 fixture compilation57 ->64 seconds; Windows2025
70 ->67 seconds. Both complete fixture jobs passed. This does not establish a hosted performance gain;
retain the limited pilot as source-mode coverage and do not expand it on the strength of the VM result alone.
The invocation switch is available independently of that CI choice, and separate remains the development default.
