# TOOLS-0017 - Limit Test Partition Defines to the Entrypoint

Status: Accepted
Date: 2026-10-27

## Context

SCTestCore and SCTestTools compile the same 40 library translation units with the same
configuration, exports and source mode. Their project-wide partition defines made their
command lines differ even though only SCTest.cpp uses those defines. Shared compiled
objects require matching compilation contracts, including all preprocessor options.

## Decision

Apply SC_TEST_EXCLUDE_TOOLS or SC_TEST_TOOLS_ONLY only to Tests/SCTest/SCTest.cpp through
an explicit per-file selection. Keep full SCTest and all other source flags unchanged.
Append this selection after the existing spaces-test selection: the latter removes
SCTest.cpp, and selection removal operates on the accumulated per-file list.

## Consequences

Library compilation contracts become identical between the two partitions. The two
entrypoint objects remain distinct and cannot be shared. This alone does not reduce
cold compilation work or authorize reuse across incompatible targets or toolchains.

Object transfer and any producer/consumer CI graph require separate validation of exact
commands, compiler/target identity, source/header dependencies, invalidation and exports.
Additional producer barriers, artifact transport and runner setup may outweigh savings.

## Confirmation

Compare emitted Debug/Release databases: all 40 common library commands must match after
normalizing only object/dependency output paths; partition defines must occur only on the
main source. Run complete and partitioned suites and standalone checks on available OSes.
