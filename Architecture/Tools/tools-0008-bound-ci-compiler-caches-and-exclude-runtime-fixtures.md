# TOOLS-0008 - Bound CI Compiler Caches and Exclude Runtime Fixtures

Status: Accepted
Date: 2026-10-25

## Context

Package caches avoid repeated toolchain downloads but do not reuse compiled objects between fresh CI
checkouts. Core, interop and experimental targets intentionally use different compilation contracts, so
combining them into one object library would erase useful compile-mode coverage.

## Decision

Pilot ccache only in the ordinary Linux Release lane. Install it from the runner's distribution packages
and prepend generated compiler-name symlinks for available GCC/Clang drivers, including the matching
versioned GNU drivers. Do not change project flags or replace packaged cross-compilers/Fil-C drivers.

Use a separate compressed cache limited to 256 MB, namespaced by cache schema, OS, architecture, image
label and configuration. Save immutable revision keys and restore compatible prior revisions. Require
compiler-content checking; retain ccache's source/header/command validation and default correctness checks.
Do not restore complete intermediate trees, test scratch directories or test outcomes.

Disable ccache during every ordinary test execution step so plugin, bootstrap, header-change, incremental
and compiler-identity fixtures still invoke real compilers. Report cache statistics even after failures,
but save the cache only after successful validation. Keep all original compile and test commands.

## Consequences

Unchanged objects can be reused across commits without merging incompatible compile modes. Cold runs pay
package/cache overhead; warm runs must demonstrate a net improvement before expanding the pilot. macOS,
Windows and experimental Fil-C compilation remain outside this compiler-cache pilot.

## Confirmation

Compare successful cold and warm executions at the same source SHA and runner image, recording cache
hits/misses, uncacheable reasons, restore/save cost and compile-step time. Verify all tests execute in both
runs and that runtime compilation has CCACHE_DISABLE=1. Changed source, headers, flags and real compiler
contents must not reuse incompatible objects.

Run CheckCompilerCache.py in an isolated temporary cache before library compilation. It verifies a warm
hit and misses after source, header, flag and configured compiler-content changes; the last probe preserves
the compiler facade's size and timestamp. Probe entries do not populate the saved cache or its statistics.

## Measured pilot result

At source a98d2b3bddf4bb910aa4ab32c11aa3806b914b95, successful Linux Release jobs in
[run 40240888911](https://github.com/Pagghiu/SaneCppLibraries/actions/runs/40240888911)
used Ubuntu 26.04 image 20261012.163.1. The cold job took 335 seconds; its planned successful-job
rerun took 153 seconds with a warm cache, saving 182 seconds (54%). Compile steps fell from
164 to 8 seconds, while test steps took 111 seconds in both attempts. These are paired hosted
samples on different machines, not guaranteed bounds.

The warm run reported 323 hits out of 323 cacheable calls and zero misses. The 125 uncacheable
calls were 63 links and 62 compiler probes without input files; these remain real executions.
The compressed cache transfer was approximately 12 MB and restore took one second. Source, header,
flag and compiler-content invalidation probes passed in both attempts. Runtime compilation was
explicitly uncached in both successful test runs.

The preceding no-cache control at e45c83424 took 315 seconds, including 161 seconds of compilation.
The cold cache therefore has a real initial cost; the demonstrated benefit depends on restoring a
compatible cache. Keep the pilot bounded to Linux Release before expanding to other compiler/backend
contracts. The per-revision restore prefix also permits compatible objects from earlier revisions;
record that separately from this same-revision measurement.
