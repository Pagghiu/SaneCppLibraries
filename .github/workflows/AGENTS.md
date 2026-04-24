# Workflow Notes

- When a workflow or `SCBuildTest` starts using a new packaged toolchain, runner, or sysroot from `Tools/SC-package.cpp`, update the GitHub Actions caches in the matching workflow in the same change.
- Prefer package-specific caches such as `_Build/_PackagesCache/llvm` or `_Build/_PackagesCache/linux-sysroot` over a single broad `_Build/_PackagesCache` cache. This keeps invalidation targeted and makes cache sizes measurable.
- If a heavy packaged dependency is needed by multiple matrix jobs in the same workflow, add a warmup job that restores the package cache, runs `SC.bat package install ...` or `./SC.sh package install ...`, saves the exact cache key, and make the matrix build jobs depend on it.
- If those matrix jobs share the same cache key, warm the cache once on a single representative runner instead of matrixing the warmup job too. Otherwise cold runs will download and extract the same large package multiple times before the build fan-out starts.
- Only restore heavy package caches in jobs that actually consume them. If a large toolchain is only needed by `Release` or by a specific test step, skipping the restore in the other matrix lanes is usually a bigger win than micro-optimizing the cache itself.
- Do not rely on a broad `restore-keys` fallback alone for newly introduced large packages. A stale fallback cache can miss the new package and every parallel matrix job will redownload it.
- `SCBuildTest` fixture runs are expected to share repository package roots for large immutable packages. Avoid adding new isolated-run package layouts for heavy real toolchains unless the test is explicitly validating package installation behavior itself.
