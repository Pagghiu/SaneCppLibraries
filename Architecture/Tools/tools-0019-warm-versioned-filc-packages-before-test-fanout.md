# TOOLS-0019 - Warm Versioned Fil-C Packages Before Test Fan-Out

Status: Accepted for measured trial
Date: 2026-10-28

## Context

Fil-C 0.687 provides compiler/runtime correctness fixes, including GC and fiber support.
The previous native pizfix pin is 0.686. Compiler-dependent zlib, OpenSSL, nghttp2 and curl
identities include the detected compiler version, so changing the compiler requires their
rebuild. Both core and tools jobs need those packages because SC-build prepares the Fil-C
runtime dependencies for every executable.

Cold preparation in both test partitions would duplicate expensive package builds.
The workflow guidelines require shared heavy packages to be warmed once before fan-out.
Caches differ by runner architecture, so x64 and ARM64 each need one producer.

## Decision

Pin the native Linux pizfix distribution to 0.687 using verified release archive hashes.
Keep both architectures, compiler flags, all test sections, TLS fixtures and coroutine checks.
Do not switch distribution flavor or claim a compiler speedup without paired measurements.
The separate glibc workflow stays pinned to 0.686; this trial retains existing rootless
fiber coverage and does not claim glibc runtime-owned-stack validation with 0.687.

Add one package warmup per architecture before the existing core/tools matrix. Probe all
four exact cache keys using lookup-only requests. If all exist, skip restore/preparation
and save work in the warmup. Otherwise restore fallbacks, install and verify the compiler
and runtime packages, and save validated packages under the exact versioned keys. Keep
consumer package validation. Existing gates fail if the warmup or any workload fails.

A prepared compiler that reports the expected version needs no patchelf installation.
Install that setup dependency only when it is absent and the expected compiler is not ready.
Check installed compiler metadata for the expected version and pizfix flavor.

After successful cold preparation on CI, prune only known obsolete compiler cache copies
for the active architecture and compiler download archives. Retain the validated active
extracted package, its metadata, other architectures and unknown paths. Do not prune local
comparison packages or delete sources from runtime dependency caches in this trial.

## Consequences

A warm run gains a lookup/queue barrier; a cold run builds heavy dependencies once per
architecture. The slower architecture's warmup gates both consumer architectures. Measure
that latency cost alongside avoided duplication and skipped system package installation.
Cache identities include the compiler version; compiler cache layout also includes the
pruning script's identity. Receipts and dependency compiler identities remain authoritative.

## Confirmation

Verify official archive digests and actual detected versions. Compare the same heavy TUs
with 0.686 and 0.687 in alternating order and keep source inputs fixed. Verify cache-key
agreement, lookup hit/miss branches, failure propagation, pruning preservation and compiler
setup behavior. Validate full native Debug/Release suites and standalone builds on available
platforms, plus real Fil-C TLS/coroutine and section inventories on both Linux architectures.
Measure cold preparation, warm lookup overhead, cache sizes and worker setup timings.
