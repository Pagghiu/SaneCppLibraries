# COMMON-0011 - Share Protected POSIX Reservations

Status: Accepted
Date: 2026-11-10

## Context

Memory, Await and Fibers each reserve inaccessible address space and commit read/write pages as needed.
With PaX MPROTECT enabled, NetBSD may reject a later read/write commit unless the reservation declares
those permissions in advance.

## Decision

Share the POSIX reservation through a private Common implementation fragment included independently by
each consumer. NetBSD starts with `PROT_NONE` and declares read/write as the maximum protection with
`PROT_MPROTECT`. Each library owns its commitment, release, and error handling.

## Confirmation

Run VirtualMemoryTest, SCAwaitTest and the Fibers tests with the guest's existing PaX policy. Regenerate
the single-file libraries and compile the consumers, including a translation unit with multiple consumers.

## Related

- [NetBSD mmap(2)](https://man.netbsd.org/mmap.2)
- [FIBERS-0045](../Fibers/fibers-0045-support-protected-and-incremental-stacks-on-bsd.md)
