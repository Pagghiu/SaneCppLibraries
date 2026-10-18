# ASYNCEXECUTION-0014 - Structured Scope Policy

Status: Proposed; implement and validate before accepting
Date: 2026-10-18

## Context

Applications need nonblocking graph ownership comparable in purpose to Await task groups and fiber counters. Standard/provider counting scopes already supply association lifetime, stop propagation and joining. Provider spawn discards values, requires a non-erroring sender, and allocates through the standard allocator contract. Directly using it would obscure SC::Result errors and cannot express a recoverable null allocation without exceptions.

## Proposed decision

Expose an immovable AsyncExecutionScope borrowing an AsyncExecutionContext. Reuse the provider counting_scope, its scope token wrapping, association and join sender. Add SC ownership/error policy rather than a second event loop or a duplicate counting/stop runtime.

- spawn(sender) returns Result after acquiring an association, allocating stable connected storage and starting the child. Successful spawn means accepted, not successfully completed. Value outputs are discarded; use ordinary provider composition to consume them within the child. Error channels must be SC::Result. Report allocation failure without starting the child; report rejection after sealing explicitly. Never access the child node after start, because synchronous completion may destroy it.
- Provide optional allocation callbacks (size/alignment plus matching deallocation), with the normal allocating default. This makes ownership and allocation failure testable without global failure hooks or imposing allocation policy on Async. Keep the allocator alive through all node reclamation.
- Each root child holds a provider association and context outstanding registration. Record success/error/stopped counts and the first observed Result error under a mutex. The selected first error follows completion order; it is not a deterministic spawn-order selection. Failed submission/allocation is returned to spawn's caller and is not counted as a started child.
- Child terminal completion snapshots its association/allocator, updates the summary, destroys the connected operation and frees storage, then drops context registration and releases the association. The provider join cannot notify before connected storage is reclaimed. No child-node access follows terminal delivery or deallocation.
- requestStop seals spawning, requests the provider scope stop source and remains nonblocking; running CPU work is cooperative. Joining also seals spawning when its sender starts, preserving lazy sender construction. A stopped child still must finish and release its association.
- join() is a sender wrapping provider join with caller stop handling and SC result policy. Caller stop requests scope stop, but never abandons the join. Unregister caller stop callbacks before terminal delivery. Emit the scope summary on success, or the first child Result error after every child has joined. Child stopped completions are visible in the summary and are not cleanup failures. Keep the summary queryable after an error completion. Permit one active join consumer; reject a concurrent join explicitly. A joined/sealed scope cannot be reopened for new children.
- Propagate scope lifetime metadata through the receiver environment to SC pool schedulers. A copied pool notification must retain a provider association until after its context notification count is dropped, so a completed join can be followed by owning close without a transient active-tail rejection. Do not reintroduce a heap ticket for ordinary scheduling.
- Scope construction does not own a loop/pool. The application drives the same context externally or through its consumer. Context/runtime close rejects outstanding scope roots, including gaps between native leaf operations. Loops, pools, descriptors and buffers still follow their borrowed lifetime contracts.
- Scope destruction requires all children and join consumers finished, with producer threads joined. Reject active destruction with an explicit assertion rather than blocking an owner loop needed by the children. A never-used empty scope is safe to destroy; after accepting children, explicitly join before destruction even if their terminal callbacks have already run (matching the provider scope contract). No unmanaged detached graph exists.

## Required confirmation

Empty and synchronous completion; concurrent pool/native completions; Result preservation and first-error/count policy; stopped children; caller-stop during join; spawn after sealing; allocation failure and over-aligned storage; terminal node self-destruction; one active join; destruction while active in a bounded isolated fixture; loop progress with scoped pool work; owning close rejection until scope quiescence. Update this ADR with the actual public types and verified boundaries before acceptance.
