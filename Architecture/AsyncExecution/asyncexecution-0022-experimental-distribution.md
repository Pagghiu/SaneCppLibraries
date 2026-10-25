# ASYNCEXECUTION-0022 - Experimental Distribution Boundary

Status: Accepted for the experiment
Date: 2026-10-25

## Decision

Keep AsyncExecution under Extra/Experimental/AsyncExecution, with local tests, examples and Markdown tutorial/reference.
Keep the SC namespace and existing target names. Dedicated targets opt into its .cpp explicitly. Core discovery,
default SC.cpp, supported-library documentation and ordinary single-file generation exclude the experiment.
No core library depends on it; experimental code may borrow core libraries. Provider selection remains explicit,
with no automatic downloads by the build. Diagnostics keep their existing reserved category solely for collision checking.

This code may change or disappear without compatibility guarantees and is not part of the supported library set.
Accepted experimental ADRs describe implementation decisions, not a support promise. Normal CI verifies the core.
The existing Posix and Windows CI workflows compile/run SCAsyncExecutionTest in Debug and Release.
They download pinned stdexec headers on a cache miss and cache them by revision and runner OS.
This checks provider interoperability;
it does not claim native standard-library conformance. No general experimental packaging framework or second
event loop is introduced.
