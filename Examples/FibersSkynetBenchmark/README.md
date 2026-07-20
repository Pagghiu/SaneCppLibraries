# Fibers Skynet Benchmark

This optional executable compares stackful `SC::FiberTask` work with Taskflow's unchanged Skynet backend from a pinned
upstream checkout. The third-party suite is not stored in this repository and ordinary builds do not download it.

```bash
./SC.sh package install taskflow-benchmarks
./SC.sh build configure FibersSkynetBenchmark
./SC.sh build run FibersSkynetBenchmark Release -- --workers 4 --rounds 5 --max-depth 4
```

The current stackful backend allows depths 1 through 4 because every live tree node owns a fixed 16 KiB stack. The
benchmark prints this preallocation policy separately from Taskflow's runtime allocation policy. A future stackless
`FiberJob` backend is the appropriate comparison for the canonical million-leaf workload.
