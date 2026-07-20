# FIBERS-0017 - Gate External Benchmarks Through Package

Status: Accepted
Date: 2026-07-20

## Context

Fibers needs recognizable comparisons with established task runtimes. Taskflow maintains a Skynet benchmark with
Taskflow, oneTBB, and OpenMP backends, but its source and dependencies are third-party assets. Vendoring that suite
would enlarge the repository, blur ownership, and make ordinary builds depend on code unrelated to the public Fibers
library.

## Decision

Install a pinned Taskflow checkout as an explicit `SC::Package` asset named `taskflow-benchmarks`. Its receipt exports
the checkout root as `taskflow-benchmarks.root`. Benchmark-only build definitions may detect this installed receipt and
compile an SC backend against the upstream workload, but `SC-build` must not download the package and ordinary Fibers
builds, tests, documentation, and single-file generation must remain independent from it.

The benchmark adapter may use the C++ standard library and third-party headers because it is an optional executable,
not library code. Results must identify the pinned revision, backend, worker count, allocation policy, and timing
boundary. Sane C++ source must not copy upstream implementation files merely to avoid the explicit install step.

## Consequences

Running external comparisons is a two-step workflow: install `taskflow-benchmarks`, then configure and build the
optional benchmark project. Updating the upstream revision is an intentional package change and cannot silently alter
historical baselines.

The Package catalog gains one Fibers-motivated asset. This is a cross-domain change and requires Package-owner review
before integration. No dependency is added from `Fibers` or `FibersAsync` to Package or Taskflow.

## Confirmation

- `SC::Package` lists `taskflow-benchmarks` as an asset and exports `taskflow-benchmarks.root`.
- The package validates the exact pinned Git commit before writing its receipt.
- A clean ordinary build does not fetch Taskflow and does not require its checkout.
- Production Fibers sources and generated single-file libraries contain no Taskflow code or include path.

## Related

- [FIBERS-0001 - Keep Fibers Independent From Async, Await, And Threading](fibers-0001-keep-fibers-independent-from-async-await-and-threading.md)
- [Fibers architecture](fibers-architecture.md)
