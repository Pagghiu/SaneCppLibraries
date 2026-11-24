# FIBERS-0045 - Support Protected and Incremental Fiber Stacks on BSD

Status: Accepted
Date: 2026-11-10

## Context

The POSIX growth handler must check the interrupted stack pointer before committing a page. NetBSD and FreeBSD
expose that pointer through different signal contexts. NetBSD with PaX MPROTECT also rejects later read/write
access to a reservation made with plain `PROT_NONE`.

## Decision

On BSD ARM64, read the interrupted pointer from NetBSD's `_UC_MACHINE_SP` or FreeBSD's `gp_sp`. On x86_64,
read NetBSD's raw `_REG_RSP` or FreeBSD's `mc_rsp`. NetBSD's x86_64 `_UC_MACHINE_SP` subtracts the ABI
red zone, while the bounds check needs the actual interrupted register. Keep the existing reservation-bound
checks, alternate signal stack, and fault forwarding from FIBERS-0034. Query the native process trace flag on
each BSD so a debugger continues to exclude incremental stack growth. Other BSD architectures leave incremental
commitment unavailable until their signal contexts are qualified.

Route Fibers' virtual allocator and stack reservations through the same private function. On NetBSD request
`PROT_NONE` with a maximum protection of read/write so the initial mapping stays inaccessible and later commits
remain permitted with PaX enabled. The maximum protection excludes execution. On both BSDs, decommit
restores no access and uses `MADV_FREE` so the kernel may discard the released pages.

## Confirmation

Run the existing Fibers and AsyncFibers suites on both BSDs. Explicitly exercise the terminal, foreign, nested, and
restored signal actions through the incremental fault section. Run the stack-growth lifecycle under a debugger
to verify that it refuses incremental growth when traced.

## OpenBSD extension

OpenBSD reports the interrupted stack pointer in `sigcontext` through `sc_rsp` on amd64 and
`sc_sp` on ARM64. Use those registers with the existing POSIX fault handling and the OpenBSD
stack mappings from FIBERS-0046. Read the process trace state through `KERN_PROC_PID` and
`PS_TRACED`.
