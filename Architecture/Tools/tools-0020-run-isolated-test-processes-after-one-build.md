# TOOLS-0020 - Run Isolated Test Processes After One Build

Status: Accepted
Date: 2026-10-29

## Context

Compiler profiles show distributed backend costs rather than a single established
language-construct cliff in the initial samples. Runtime remains a possible latency
saving without changing compiler optimization, instrumentation or test bodies.

## Decision

Keep SCTest's default registration order and behavior. Add opt-in shard routing in
its entrypoint, before constructors execute, using the registration function name
and a stable hash. Preserve every registration occurrence, including duplicates.
Expose a list-only inventory and reject malformed shard arguments. No public
Testing library API or assertion/section behavior changes.

Require an existing absolute scratch root for sharded execution. Use its physical
working-directory path for fixtures while preserving the actual executable and
library roots; macOS FSEvents cannot match an aliased /tmp spelling. Each process
receives a separate root and port range. The pilot reserves test listening bands
5000..8200 and 26000..26200, shifted by 4000 for the second process. It validates
disjoint bands below the host's ephemeral client-port range, and checks literal
test ports against the audited bands. Base offsets are limited to 0..2000.
Variable ports in the existing accept/keepalive scenarios were audited too;
new variable-port scenarios must preserve these bounds. A wider 22000 shift was
rejected after entering Linux's ephemeral range and causing a bind failure.
Keep both roots and logs.

Compile once first. Invoke the framework through SC.sh once, prepare runtime packages,
then let SCTest launch its own executable through Process for both groups and
retain each exit status. This follows existing self-process test scenarios and
avoids concurrent package smoke compiles at shared output paths. The parent
emits the full registration inventory before launching children; no separate
framework startup is needed for discovery. Validate the exact registration
multiset, and compare successful-section occurrence counts in controlled serial
versus parallel trials. No retries, timeouts, test exclusions, new runner jobs or
compiler-flag changes. Controlled hosted comparisons passed on all six Posix
execution paths, preserving exact registrations and section occurrences. Use two
processes in existing native/Xcode and Fil-C core jobs; local default invocation
stays serial. Fil-C's normal fixture still requires exactly three TLS connections
and one request; only the manual double-suite comparison expects six and two.
Seed nested/excluded iterator fixtures in each scratch root so coverage does not
depend on incidental output-directory contents. Upload detailed artifacts for
failures and manual comparisons; successful normal runs retain results in logs.

## Confirmation

Check default and sharded Debug/Release, invalid options, child-process scenarios,
filesystem/port isolation, duplicate registrations and exact section inventories.
Keep Windows launcher validation sequential. Measure startup/package/runner overhead
and the whole critical path before expanding the pilot. Preserve all-tests, real
TLS, coroutine and platform/compiler coverage.
