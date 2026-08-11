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
stackless backend allows depths 1 through 6 and publishes a distinct stable continuation job when an internal node's
last child finishes, making it the appropriate SC comparison for the canonical million-leaf workload. Each depth keeps
one persistent worker pool across warm-up and measured waves. Both SC backends assign child nodes to deterministic
breadth-first slots, avoiding a benchmark-only contended allocation counter. Run the stackless backend directly with:

```bash
./SC.sh build run FibersSkynetBenchmark Release -- --backend jobs --workers 4 --rounds 5 --max-depth 6
```

Use up to 100 measured rounds for longer captures. `--job-diagnostics` adds each sample's minimum and maximum jobs
executed by one worker plus aggregate steal attempts, successful steals, and failed steals. The benchmark prints both
SC preallocation policies separately from Taskflow's runtime allocation policy.
