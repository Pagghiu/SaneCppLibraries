@page page_tests Tests

- Each library has an associated test in its `Tests` subfolder.
- Many tests also serve as executable examples and useful reference code when learning a library.
- The project aims for at least 90% code coverage where practical.
- Coverage reports are generated from `main` when the website is published and are available in the [coverage report](https://pagghiu.github.io/SaneCppLibraries/reference/doxygen/coverage/).
- See [Building (Contributor)](@ref page_building_contributor) to build, run, or debug the tests locally.

Posix CI runs SCTest in two isolated groups after compilation, with disjoint
ports and scratch directories. The default local test invocation remains serial. To compare both modes, including the exact registration
and successful-section occurrence counts:

```sh
./SC.sh build compile SCTest Release
python3 Support/Scripts/RunSCTestShards.py --configuration Release --compare-serial --output _Build/TestShardTrial
```

Omit `--compare-serial` to run the suite once across both processes, as CI does.
Choose a new output directory for each trial. It retains logs, child exit statuses
and scratch-root paths. The framework prepares runtime packages once before SCTest
starts its own children; the runner preserves `--all-tests` and compiler settings.
Use `--project SCTestCore` for the explicit core target or `--generator xcode` for
an already compiled Xcode target. See TOOLS-0020 in `Architecture/Tools` for the
isolation and coverage contract.

Add `--timings` to record monotonic execution milliseconds for each registration,
including duplicates, in the retained summary. Manual CI comparisons enable this
diagnostic; ordinary test invocations leave it disabled. It measures test execution
separately from the framework startup included in the overall run duration.

Timing diagnostics also sample existing flushed section output at 50 ms intervals
to locate long waits inside a registration. These observations include polling and
stdio delivery uncertainty; they are not precise per-section execution timings.
They do not change the test code or its scheduling.
