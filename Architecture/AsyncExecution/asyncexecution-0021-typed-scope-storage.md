# ASYNCEXECUTION-0021 - Typed Scope Root Storage

Status: Accepted
Date: 2026-10-20

## Decision

Provide immovable AsyncExecutionScopeSlot<Sender> and AsyncExecutionScopePool<Sender>. Slots contain correctly aligned uninitialized storage for the full decayed sender's scope-wrapped connected child. Pools borrow homogeneous slot arrays, maintain intrusive free metadata and synchronize reservation/release using SC mutexes. No byte arena, automatic growth, payload arrays or heap spill is hidden in these facilities.

spawn(slot, sender) and spawn(pool, sender) reserve before consuming/connecting/starting the sender. Busy and capacity errors are distinct. Pool-bound slots reject direct use. Rejection does not count an accepted root or keep a provider association. Publish reuse only after child destruction and its last slot access, before releasing the association that can finish join. No lock is held across receiver invocation or association release. Sender start may synchronously destroy its child; never touch the node after start.

Default scopes configure no allocating policy. spawn(sender) requires an explicit complete callback pair; AsyncExecutionScopeAllocator::heap() provides named heap opt-in solely for execution nodes. Typed overloads never spill into it. This supersedes ADR 0013's implicit allocating default; its stop, result, context-accounting and mandatory-join policies remain.

A pool supports one exact decayed graph type. Named factories stabilize types; separately written lambdas can require different pools. Multiple heterogeneous slots/pools may feed one scope. Local run/runInto and caller-owned raw connect/start need no new storage wrapper. Scope joining seals submissions; storage may be reused with another scope after join. Payload reuse/admission remains application policy.

## Consequences

Capacity and sizeof(slot) are visible; graph/provider changes can change slot size/alignment. Templates cover type-dependent layout only; common reservation, accounting and heap callbacks compile in the .cpp. Pool close rejects active slots; pool/slot/scope destruction requires joined work and finished producers. Arbitrary open-ended root storage can opt into allocation explicitly. No universal allocator is required merely because a scope accepts different graphs.
