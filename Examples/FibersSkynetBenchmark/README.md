# Fibers Skynet Benchmark

This optional executable compares stackful `SC::FiberTask`, stackless `SC::FiberJob`, and Taskflow's unchanged Skynet
backend from a pinned upstream checkout. The third-party suite is not stored in this repository and ordinary builds do
not download it.

```bash
./SC.sh package install taskflow-benchmarks
./SC.sh build configure FibersSkynetBenchmark
./SC.sh build run FibersSkynetBenchmark Release -- --workers 4 --rounds 5 --max-depth 4
```

The current stackful backend allows depths 1 through 4 because every live tree node owns a fixed 16 KiB stack. The
stackless backend allows depths 1 through 6 and republishes an internal node as a continuation when its last child
finishes, making it the appropriate SC comparison for the canonical million-leaf workload. Run it directly with:

```bash
./SC.sh build run FibersSkynetBenchmark Release -- --backend jobs --workers 4 --rounds 5 --max-depth 6
```

The benchmark prints both SC preallocation policies separately from Taskflow's runtime allocation policy.
