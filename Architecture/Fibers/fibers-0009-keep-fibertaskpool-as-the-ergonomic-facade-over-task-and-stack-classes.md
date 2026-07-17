# FIBERS-0009 - Keep FiberTaskPool As The Ergonomic Facade Over Task And Stack Classes

Status: Accepted
Date: 2026-07-10

## Context

Large bounded workloads need independently scalable `FiberTask` records and virtual stack slots. Exposing only the
separate `FiberTaskClass` and `FiberStackClass` APIs would make the normal spawn path require callers to coordinate two
acquisitions, two releases, rollback, availability waiting, and post-root-context reuse safety.

At the same time, some advanced callers need direct control over task or stack classes and must not be forced through a
heap-backed or shared-ownership facade.

## Decision

Keep `FiberTaskPool` as the normal coupled facade over one `FiberTaskClass` and one `FiberStackClass`.

`FiberTaskPool::spawn()` acquires task storage first, acquires stack storage second, rolls the task acquisition back if
stack acquisition fails, and returns stack storage only after the completed fiber has returned to its worker root
context. Task storage normally returns at that same point. A task spawned through `FiberTaskGroup` is the explicit
exception from FIBERS-0015: its task record, including a class-backed task slot, remains retained until the completed
group wave is reset. Pool capacity and `waitForSpawnCapacity()` describe the ability to attempt one coupled spawn; they
do not reserve a future slot.

Retain direct `FiberTaskClass` and `FiberStackClass` APIs for advanced callers. They remain explicit, fixed-capacity,
and caller-owned, and they do not introduce handles, shared ownership, or hidden allocation.

## Consequences

Most users have one bounded API for reusable fiber work without losing explicit storage control. The scheduler has one
well-defined place to preserve task/stack pairing and safe delayed reuse. Advanced use remains possible, but direct
class users own their more detailed lifetime coordination.

Grouped work can temporarily retain task capacity after execution storage is reusable. This is the explicit cost of
stable allocation-free result inspection and is released by `FiberTaskGroup::reset()`.

Multiple stack sizes are represented by multiple pools or explicit class composition rather than making one pool choose
an implicit stack class per spawn.

## Confirmation

A change preserves this decision when pooled spawn either owns both a task and a stack or neither, failed stack
acquisition returns the task slot, completed pooled fibers release reusable storage only after switching to the worker
root context, and the facade does not hide allocation or lifetime ownership. Grouped task records and class-backed task
slots remain retained until group reset as required by FIBERS-0015, while their stack slots still release at
root-context completion.

## Related

- [FIBERS-0002 - Use explicit FiberAllocator storage for scalable runtime memory](fibers-0002-use-explicit-fiberallocator-storage-for-scalable-runtime-memory.md)
- [FIBERS-0003 - Keep task and stack lifetimes caller-owned and memory-stable](fibers-0003-keep-task-and-stack-lifetimes-caller-owned-and-memory-stable.md)
- [FIBERS-0007 - Model spawn backpressure as explicit capacity waiting](fibers-0007-model-spawn-backpressure-as-explicit-capacity-waiting.md)
- [FIBERS-0015 - Retain task group records until explicit reset](fibers-0015-retain-task-group-records-until-explicit-reset.md)
- [Fibers architecture](fibers-architecture.md)
